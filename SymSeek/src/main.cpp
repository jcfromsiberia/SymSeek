#include "MainWindow.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

int main(int argc, char ** argv)
{
    QApplication app(argc, argv);

    // Necessary evil for the settings mechanism
    app.setOrganizationName("jcfromsiberia");
    app.setApplicationName("SymSeek");

    // Same look and feel on all platforms
    app.setStyle(QStyleFactory::create("Fusion"));

    MainWindow mw;
    mw.show();

    return app.exec();
}