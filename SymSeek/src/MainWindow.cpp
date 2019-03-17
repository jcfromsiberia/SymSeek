#include "MainWindow.h"

#include "ui_mainwindow.h"

#include <QtGui/QRegExpValidator>

#include <symseek/symseek.h>

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

    using namespace SymSeek;

    QStringList masks =
#if defined(Q_OS_WIN)
        {"*.dll", "*.exe"}
#elif defined(Q_OS_LINUX)
        {"*.so"}
#endif
        ;
    auto symbols = findSymbols(directory, {"*.dll", "*.exe"},
                               [&symbolName](Symbol const &symbol) -> SymbolHandlerAction {
                                   return symbol.demangledName.contains(symbolName)
                                          ? SymbolHandlerAction::Add
                                          : SymbolHandlerAction::Skip;
                               });
    m_model.setSymbols(symbols);
}

MainWindow::~MainWindow()
{
}
