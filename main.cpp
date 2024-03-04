#include "mainwindow.h"

#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;

    if (translator.load(QLocale(),
                        QLatin1String("quickimport"),
                        QLatin1String("_"),
                        QLatin1String(":/translation")))
        QCoreApplication::installTranslator(&translator);

    // QPixmap image("://QuickImportLogo-klein.png");
    // a.setWindowIcon(image);
    MainWindow w;

    // w.setWindowIcon(image);
    w.show();

    return a.exec();
}
