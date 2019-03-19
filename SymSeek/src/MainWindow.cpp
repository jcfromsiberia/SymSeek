#include "MainWindow.h"

#include "ui_mainwindow.h"

#include <QtCore/QEventLoop>
#include <QtCore/QMetaObject>
#include <QtGui/QRegExpValidator>

#include <symseek/symseek.h>

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
{
    m_ui->setupUi(this);

    // Connections
    QObject::connect(m_ui->pbSearch, &QPushButton::clicked, this, &MainWindow::doSearch);
    m_ui->tvResults->setModel(&m_model);
    auto hdr = m_ui->tvResults->horizontalHeader();
    hdr->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(4, QHeaderView::Stretch);
}

void MainWindow::doSearch()
{
    auto directory  = m_ui->leDirectory->text();
    auto symbolName = m_ui->leSymbolName->text();
    m_ui->pbSearch->setEnabled(false);

    using namespace SymSeek;

    QStringList masks =
#if defined(Q_OS_WIN)
        {"*.dll", "*.exe"}
#elif defined(Q_OS_LINUX)
        {"*.so"}
#endif
        ;

    AsyncSeeker asyncSeeker{ directory, masks, [symbolName](Symbol const & symbol) {
        return symbol.demangledName.contains(symbolName)
               ? SymbolHandlerAction::Add
               : SymbolHandlerAction::Skip;
    } };

    auto seeker = asyncSeeker.seeker();

    // Stupid Qt boilerplate!
    qRegisterMetaType<size_t>("size_t");
    qRegisterMetaType<SymbolSeeker::ProgressStatus>("SymSeek::SymbolSeeker::ProgressStatus");

    connect(seeker, &SymbolSeeker::startProcessingItems, /*context=*/this,
            [this](size_t itemsCount)  {
            //TODO Setup QProgressBar
        });
    connect(seeker, &SymbolSeeker::itemsRemaining, /*context=*/this,
            [this](size_t itemsCount) {
            //TODO Setup QProgressBar
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

    QEventLoop loop;
    connect(&asyncSeeker, &AsyncSeeker::finished, &loop, &QEventLoop::quit);
    asyncSeeker.start();
    loop.exec();

    statusBar()->showMessage("Finished", 3000);
    m_model.setSymbols(asyncSeeker.result());
    m_ui->pbSearch->setEnabled(true);

}

MainWindow::~MainWindow()
{
}
