#include "MainWindow.h"

#include <QtCore/QSettings>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QToolButton>

#include <ui_mainwindow.h>

#include "Debug.h"

namespace
{
    QString const geometrySetting  = QStringLiteral("gui/windowGeometry");
    QString const tabsCountSetting = QStringLiteral("gui/tabsCount");
}

MainWindow::MainWindow(QWidget * parent)
: QMainWindow{ parent }
, m_ui{ std::make_unique<QT_PREPEND_NAMESPACE(Ui::MainWindow)>() }
{
    m_ui->setupUi(this);

    // Customize UI
    QToolButton * toolButton{ new QToolButton(m_ui->tabWidget) };
    // TODO Add a normal icon
    toolButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    toolButton->setToolTip("Add new workspace");
    m_ui->tabWidget->addTab(new QWidget(m_ui->tabWidget), QString{});
    m_ui->tabWidget->setTabEnabled(0, false);
    m_ui->tabWidget->tabBar()->setTabButton(0, QTabBar::RightSide, toolButton);

    // Setup connections
    connect(toolButton, &QToolButton::clicked, this, &MainWindow::addWorkspace);
    connect(m_ui->tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::disposeTab);

    loadSettings();
}

MainWindow::~MainWindow()
{
    storeSettings();
}

int MainWindow::addWorkspace()
{
    QPointer<Workspace> newWorkspace{ new Workspace(m_ui->tabWidget) };
    // Always insert before the last (plus) tab
    int index = m_ui->tabWidget->count() - 1;
    m_ui->tabWidget->insertTab(index,
            newWorkspace.data(), QStringLiteral("Workspace ") + QString::number(index + 1));

    // Update tab titles
    connect(newWorkspace, &Workspace::titleChanged, /*context=*/this,
        [this, newWorkspace](QString newTitle) {
            // Index may have changed as the tabs are movable, so obtaining it again
            int index = m_ui->tabWidget->indexOf(GUARD(newWorkspace));
            Q_ASSERT(index != -1);
            m_ui->tabWidget->setTabText(index, newTitle);
            m_ui->tabWidget->setTabToolTip(index, newTitle);
        }
    );

    newWorkspace->loadSettings(index);
    return index;
}

void MainWindow::disposeTab(int index)
{
    // Leave at least one open workspace
    if(m_ui->tabWidget->count() == 2)
       return;
    QSettings settings;
    settings.remove(QStringLiteral("gui/tab") + QString::number(index));
    m_ui->tabWidget->removeTab(index);
    m_ui->tabWidget->setCurrentIndex(m_ui->tabWidget->count() - 2);
}

void MainWindow::loadSettings()
{
    QSettings settings;
    if(QByteArray const geometry = settings.value(geometrySetting).toByteArray(); !geometry.isEmpty())
        restoreGeometry(geometry);

    int tabsCount = settings.value(tabsCountSetting, 1).toInt();
    for(int i = 0; i < tabsCount; ++i)
    {
        addWorkspace();
    }
    m_ui->tabWidget->setCurrentIndex(0);
}

void MainWindow::storeSettings() const
{
    QSettings settings;
    settings.setValue(tabsCountSetting, m_ui->tabWidget->count() - 1);
    for(int i = 0; i < m_ui->tabWidget->count() - 1; ++i)
    {
        auto form = GUARD(qobject_cast<Workspace*>(m_ui->tabWidget->widget(i)));
        form->storeSettings(i);
    }
    settings.setValue(geometrySetting, saveGeometry());
}
