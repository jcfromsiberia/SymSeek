#pragma once

#include <memory>

#include <QtCore/QPointer>
#include <QtWidgets/QMainWindow>

#include "SymbolsModel.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget * parent = nullptr);

    void doSearch();

    ~MainWindow();

private:
    std::unique_ptr<Ui::MainWindow> m_ui;
    SymbolsModel m_model;
};
