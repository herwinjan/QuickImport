
#include "cardautostart.h"
#include "language.h"
#include "mainwindow.h"

#include <QApplication>
#include <QImageReader>
#include <QDebug>
#include <QThread>
#include <QObject>
#include <QCoreApplication>
#include <QStorageInfo>

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

    // Started by the card-autostart LaunchAgent. It fires on every change
    // to /Volumes (any mount or unmount), so only carry on when a memory
    // card is really there; give the mount a few seconds to complete.
    if (a.arguments().contains(CardAutostart::launchArgument())) {
        QList<QStorageInfo> cards;
        for (int attempt = 0; attempt < 20 && cards.isEmpty(); ++attempt) {
            if (attempt > 0)
                QThread::msleep(250);
            cards = MainWindow::mountedCards();
        }
        // Forget cards that are no longer mounted, then quit unless there
        // is a card we have not shown yet. The agent also fires for writes
        // inside mounted volumes (deleting imported files from the card,
        // writing to a network share), so most starts are not insertions.
        CardAutostart::pruneHandled(cards);
        bool newCard = false;
        for (const QStorageInfo &card : cards)
            newCard = newCard || !CardAutostart::wasHandled(card);
        if (!newCard) {
            qDebug() << "Started for a card insertion but there is no new card; quitting";
            return 0;
        }
    }
    // Keep the LaunchAgent pointing at this bundle after a move or update
    CardAutostart::refresh();

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
