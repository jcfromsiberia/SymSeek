#include "MainWindow.h"

#include <QtWidgets/QApplication>

int main(int argc, char ** argv)
{
    QApplication app(argc, argv);

    // Necessary evil for the settings mechanism
    app.setOrganizationName("jcfromsiberia");
    app.setApplicationName("SymSeek");

    MainWindow mw;
    mw.show();

    return app.exec();
}