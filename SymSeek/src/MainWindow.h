#pragma once

#include <memory>

#include <QtCore/QPointer>
#include <QtWidgets/QMainWindow>

#include "Workspace.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

namespace SymSeek::QtUI
{
    class MainWindow: public QMainWindow
    {
        Q_OBJECT

    public:
        MainWindow(QWidget * parent = nullptr);
        ~MainWindow();

    private:
        void loadSettings();
        void storeSettings() const;

        int addWorkspace();
        void disposeTab(int index);

    private:
        std::unique_ptr<Ui::MainWindow> m_ui;
    };
}
