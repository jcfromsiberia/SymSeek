#include "MainWindow.h"

#include "ui_mainwindow.h"

#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtGui/QValidator>
#include <QtWidgets/QFileDialog>

#include <symseek/symseek.h>

#include "Debug.h"

class CallbackValidator: public QValidator
{
public:
    using Callback = std::function<bool(QString const &)>;

    CallbackValidator(Callback callback, QObject * parent = nullptr)
    : QValidator(parent)
    , m_callback(GUARD(callback))
    {
    }

    State validate(QString & input, int & pos) const override
    {
        return m_callback(input) ? QValidator::Acceptable : QValidator::Intermediate;
    }

private:
    Callback m_callback;
};

AsyncSeeker::AsyncSeeker(QString const & directory, QStringList const & masks,
                         SymSeek::SymbolHandler handler, QObject * parent)
: QThread{ parent }
, m_directory{ directory }
, m_masks    { masks     }
, m_handler  { handler   }
{
    m_seeker.moveToThread(this);
}

void AsyncSeeker::run()
{
    m_result = m_seeker.findSymbols(m_directory, m_masks, m_handler);
}

SymSeek::SymbolSeeker const * AsyncSeeker::seeker() const
{
    return &m_seeker;
}

SymSeek::SymbolSeeker * AsyncSeeker::seeker()
{
    return &m_seeker;
}

QVector<SymSeek::SymbolsInBinary> AsyncSeeker::result() const
{
    return m_result;
}

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent)
, m_ui(std::make_unique<Ui::MainWindow>())
, m_model{ this }
, m_proxyModel{ this }
{
    m_ui->setupUi(this);

    // Models setup
    m_proxyModel.setSourceModel(&m_model);
    m_ui->tvResults->setModel(&m_proxyModel);
    m_ui->tvResults->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    m_ui->tvResults->setSortingEnabled(true);

    // Validators setup
    m_directoryValidator = new CallbackValidator{
            [](QString const & path) {
                return !path.isEmpty() && QDir{ path }.exists();
            }, this };
    m_regexValidator = new CallbackValidator{
            [](QString const & pattern) {
                return QRegExp{ pattern }.isValid();
            }
    };
    m_ui->leDirectory->setValidator(m_directoryValidator);

    // UI setup
    loadSettings();
    auto hdr = m_ui->tvResults->horizontalHeader();
    hdr->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(4, QHeaderView::Stretch);

    m_ui->pbProgress->hide();

    // Connections
    connect(m_ui->pbSearch, &QPushButton::clicked, this, &MainWindow::doSearch);
    connect(m_ui->pbChooseDir, &QPushButton::clicked, [this]() {

        QString currentPath = m_ui->leDirectory->text();
        if(!QDir{currentPath }.exists())
            currentPath = QDir::currentPath();

        QFileDialog dialog;
        dialog.setDirectory(currentPath);
        dialog.setWindowTitle("Open Directory");
        dialog.setFileMode(QFileDialog::DirectoryOnly);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dialog.setOption(QFileDialog::ShowDirsOnly, false);
        if(dialog.exec() == QDialog::Rejected)
            return;

        QDir dir = dialog.directory();
        if(dir.exists())
            m_ui->leDirectory->setText(dir.path());
    });
    connect(m_ui->chbRegex, &QCheckBox::stateChanged, [this](int state) {
        m_ui->leSymbolName->setValidator(state == Qt::Checked ? m_regexValidator : nullptr);
    });
}

static void flashWidget(QWidget * widget)
{
    QString styleSheet = GUARD(widget)->styleSheet();
    widget->setStyleSheet("border: 2px solid red;");
    widget->repaint();
    QTimer::singleShot(500, [styleSheet, widget]() {
        widget->setStyleSheet(styleSheet);
        widget->repaint();
    });
}

void MainWindow::doSearch()
{
    if(!m_ui->leDirectory->hasAcceptableInput())
    {
        flashWidget(m_ui->leDirectory);
        return;
    }
    if(!m_ui->leSymbolName->hasAcceptableInput())
    {
        flashWidget(m_ui->leSymbolName);
        return;
    }
    auto const directory = m_ui->leDirectory->text();
    auto const globs = m_ui->leGlobs->text();
    auto const symbolName = m_ui->leSymbolName->text();
    auto const isRegex = m_ui->chbRegex->isChecked();

    using namespace SymSeek;

    QStringList const masks = globs.split(
#if defined(Q_OS_WIN)
        ';'
#else
        ':'
#endif
    );

    QRegExp symbolRx{ symbolName };
    AsyncSeeker asyncSeeker{ directory, masks, [symbolName, isRegex, &symbolRx](Symbol const & symbol) {
        return (isRegex ? symbolRx.indexIn(symbol.demangledName) > -1 : symbol.demangledName.contains(symbolName))
               ? SymbolHandlerAction::Add
               : SymbolHandlerAction::Skip;
    } };

    auto seeker = asyncSeeker.seeker();

    // Stupid Qt boilerplate!
    qRegisterMetaType<size_t>("size_t");
    qRegisterMetaType<SymbolSeeker::ProgressStatus>("SymSeek::SymbolSeeker::ProgressStatus");

    connect(seeker, &SymbolSeeker::startProcessingItems, m_ui->pbProgress, &QProgressBar::setMaximum);
    connect(seeker, &SymbolSeeker::itemsRemaining, /*context=*/this,
            [this](size_t itemsCount) {
                m_ui->pbProgress->setValue(int(m_ui->pbProgress->maximum() - itemsCount));
        });

    connect(seeker, &SymbolSeeker::itemStatus, /*context=*/this,
            [this](QString binary, SymbolSeeker::ProgressStatus status) {
            QString statusText;
            switch(status)
            {
                case SymbolSeeker::ProgressStatus::Start:
                    statusText = QStringLiteral("In Progress %1").arg(binary);
                    break;
                case SymbolSeeker::ProgressStatus::Reject:
                    statusText = QStringLiteral("Rejected %1").arg(binary);
                    break;
                case SymbolSeeker::ProgressStatus::Finish:
                    statusText = QStringLiteral("Finished %1").arg(binary);
                    break;
            };
            statusBar()->showMessage(statusText);
        });
    bool interrupted = false;
    connect(seeker, &SymbolSeeker::interrupted, /*context=*/this,
            [&interrupted]()
            {
                interrupted = true;
            });

    auto searchBtn = m_ui->pbSearch;
    QString buttonText = searchBtn->text();
    searchBtn->setText("Stop");
    disconnect(searchBtn, &QPushButton::clicked, this, &MainWindow::doSearch);
    connect(searchBtn, &QPushButton::clicked, seeker, &SymbolSeeker::interrupt);

    QEventLoop loop;
    connect(&asyncSeeker, &AsyncSeeker::finished, &loop, &QEventLoop::quit);
    asyncSeeker.start();
    m_ui->pbProgress->show();
    loop.exec();

    QString finishedMessage = "Finished";
    if(interrupted) finishedMessage = "Interrupted";
    statusBar()->showMessage(finishedMessage, 3000);
    m_model.setSymbols(asyncSeeker.result());

    connect(searchBtn, &QPushButton::clicked, this, &MainWindow::doSearch);
    searchBtn->setText(buttonText);
    m_ui->pbProgress->hide();
}

MainWindow::~MainWindow()
{
    storeSettings();
}

namespace
{
    // Names for settings
    QString const guiGroup          = QStringLiteral("gui");
    QString const geometrySetting   = QStringLiteral("windowGeometry");
    QString const directorySetting  = QStringLiteral("directory");
    QString const globsSetting      = QStringLiteral("globs");
    QString const symbolNameSetting = QStringLiteral("symbolName");
}

void MainWindow::loadSettings()
{
    m_settings.beginGroup(guiGroup);
    if(QByteArray const geometry = m_settings.value(geometrySetting).toByteArray(); !geometry.isEmpty())
        restoreGeometry(geometry);

    m_ui->leDirectory->setText(m_settings.value(directorySetting).toString());
    m_ui->leGlobs->setText(m_settings.value(globsSetting,
            QString{
// TODO Get these globs from the available image parsers
#if defined(Q_OS_WIN)
// TODO Add static libs support (*.lib for MSVS linker) and *.a for GNU ld
                      "*.dll;*.exe"
#elif defined(Q_OS_LINUX)
                      "*"
#endif
            }
    ).toString());
    m_ui->leSymbolName->setText(m_settings.value(symbolNameSetting).toString());
    m_settings.endGroup();
}

void MainWindow::storeSettings() const
{
    m_settings.beginGroup(guiGroup);

    m_settings.setValue(geometrySetting, saveGeometry());
    m_settings.setValue(directorySetting, m_ui->leDirectory->text());
    m_settings.setValue(globsSetting, m_ui->leGlobs->text());
    m_settings.setValue(symbolNameSetting, m_ui->leSymbolName->text());

    m_settings.endGroup();
}
