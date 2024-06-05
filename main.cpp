
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

    qDebug() << "From main thread: " << QThread::currentThreadId();

    QDeviceWatcher *watcher;

    watcher = new QDeviceWatcher;
    watcher->appendEventReceiver(&w);
    MainWindow::connect(watcher,
                        SIGNAL(deviceAdded(QString)),
                        &w,
                        SLOT(slotDeviceAdded(QString)),
                        Qt::DirectConnection);
    MainWindow::connect(watcher,
                        SIGNAL(deviceChanged(QString)),
                        &w,
                        SLOT(slotDeviceChanged(QString)),
                        Qt::DirectConnection);
    MainWindow::connect(watcher,
                        SIGNAL(deviceRemoved(QString)),
                        &w,
                        SLOT(slotDeviceRemoved(QString)),
                        Qt::DirectConnection);

    watcher->start();
    w.show();

    qDebug() << "started";
    return a.exec();
}
