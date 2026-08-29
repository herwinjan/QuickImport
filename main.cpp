
#include "language.h"
#include "mainwindow.h"

#include <QApplication>
#include <QImageReader>
#include <QDebug>
#include <QThread>
#include <QObject>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setApplicationName("QuickImport");
    // Single source of truth: the version is set in CMakeLists.txt (project VERSION)
    QCoreApplication::setApplicationVersion(QUICKIMPORT_VERSION);
    QCoreApplication::setOrganizationName("HJ Steehouwer");
    // Uses the "language" setting, falling back to the system locale.
    AppLanguage::install();

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
