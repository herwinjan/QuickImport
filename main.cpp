
#include "mainwindow.h"

#include <QApplication>
#include <QImageReader>
#include <QTranslator>
#include <QDebug>
#include <QThread>
#include <QObject>
#include <QCoreApplication>

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

    qDebug() << "From main thread: " << QThread::currentThreadId();
    

    QDeviceWatcher *watcher;
    
    watcher = new QDeviceWatcher(&w);
    watcher->appendEventReceiver(&w);
    
    QObject::connect(watcher, &QDeviceWatcher::deviceAdded,
                     &w, &MainWindow::slotDeviceAdded,
                     Qt::QueuedConnection);
    QObject::connect(watcher, &QDeviceWatcher::deviceChanged,
                     &w, &MainWindow::slotDeviceChanged,
                     Qt::QueuedConnection);
    QObject::connect(watcher, &QDeviceWatcher::deviceRemoved,
                     &w, &MainWindow::slotDeviceRemoved,
                     Qt::QueuedConnection);

    watcher->start();

    w.show();

    qDebug() << "started";
    return a.exec();
}
