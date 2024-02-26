#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // QPixmap image("://QuickImportLogo-klein.png");
    // a.setWindowIcon(image);
    MainWindow w;

    // w.setWindowIcon(image);
    w.show();
    return a.exec();
}
