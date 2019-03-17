#include "MainWindow.h"

#include <QtWidgets/QApplication>
#include <QtCore/QDebug>

int main(int argc, char ** argv)
{
    QApplication app(argc, argv);

    MainWindow mw;
    mw.show();

    return app.exec();
}