
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

// Started by the card-autostart LaunchAgent? It fires for every change
// under /Volumes (mounts, unmounts, and writes inside any mounted volume),
// so most starts are not insertions; the agent already checks that a card
// is mounted and that no QuickImport is running, and this decides whether
// that card is one we have not shown yet. It runs before QApplication
// exists on purpose: without an NSApplication there is no Dock icon, so a
// start that ends here is invisible instead of a bouncing icon.
static bool startedForCard(int argc, char *argv[])
{
    const QByteArray flag = CardAutostart::launchArgument().toUtf8();
    for (int i = 1; i < argc; ++i)
        if (flag == argv[i])
            return true;
    return false;
}

static bool newCardMounted()
{
    QList<QStorageInfo> cards;
    for (int attempt = 0; attempt < 20 && cards.isEmpty(); ++attempt) {
        if (attempt > 0)
            QThread::msleep(250); // give the mount a few seconds to complete
        cards = MainWindow::mountedCards();
    }
    // Forget cards that are no longer mounted, then look for one we have
    // not shown yet.
    CardAutostart::pruneHandled(cards);
    for (const QStorageInfo &card : cards)
        if (!CardAutostart::wasHandled(card))
            return true;
    return false;
}

int main(int argc, char *argv[])
{
    // Needed by QSettings (which does not require a QCoreApplication
    // instance); the version is set once QApplication exists, below.
    QCoreApplication::setApplicationName("QuickImport");
    QCoreApplication::setOrganizationName("HJ Steehouwer");

    if (startedForCard(argc, argv) && !newCardMounted()) {
        qDebug() << "Started for a card insertion but there is no new card; quitting";
        return 0;
    }

    QApplication a(argc, argv);

    // Single source of truth: the version is set in CMakeLists.txt (project VERSION)
    QCoreApplication::setApplicationVersion(QUICKIMPORT_VERSION);
    // Uses the "language" setting, falling back to the system locale.
    AppLanguage::install();

    // QPixmap image("://QuickImportLogo-klein.png");
    // a.setWindowIcon(image);

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
