#include "cardautostart.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStorageInfo>
#include <QStandardPaths>

#ifdef Q_OS_MACOS
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <unistd.h>
#endif

namespace {

const QString kLabel = QStringLiteral("nl.steehouwer.quickimport.cardwatch");
const QString kArgument = QStringLiteral("--card-inserted");

#ifdef Q_OS_MACOS
QString plistPath()
{
    return QDir::homePath() + QStringLiteral("/Library/LaunchAgents/") + kLabel
           + QStringLiteral(".plist");
}

// launchd's WatchPaths on /Volumes fires not only for mounts and unmounts
// but for every write or delete inside any mounted volume (verified on
// macOS 26: deleting a file on the card or writing to an SMB share both
// trigger it; reading does not). During an import that means an event
// every few seconds. So the agent (1) only runs `open` when no QuickImport
// process exists — `open -a` on a running app would yank it to the front
// each time — and (2) the app itself remembers which card mounts it has
// already shown (handledKey/markHandled below), so a later spurious start
// for the same card quits again silently.

// .../QuickImport.app — empty when not running from a bundle (e.g. a bare
// build-tree executable), in which case there is nothing sensible to launch.
QString bundlePath()
{
    QDir dir(QCoreApplication::applicationDirPath()); // Contents/MacOS
    if (!dir.cdUp() || !dir.cdUp())
        return QString();
    const QString path = dir.absolutePath();
    return path.endsWith(QStringLiteral(".app")) ? path : QString();
}

QString xmlEscape(QString s)
{
    return s.replace(QLatin1Char('&'), QStringLiteral("&amp;"))
            .replace(QLatin1Char('<'), QStringLiteral("&lt;"))
            .replace(QLatin1Char('>'), QStringLiteral("&gt;"));
}

// What launchd runs, as a /bin/sh -c script with the bundle path in $0.
// Starting the app just to have it discover there is no card makes the
// Dock icon bounce for five seconds on every write to a network share, so
// the checks that need no Qt happen here: do nothing while QuickImport is
// running, and only open it when a writable FAT/exFAT volume is mounted.
// The mount may complete a moment after launchd fires, hence the retries.
QString agentScript()
{
    return QStringLiteral(
        "if pgrep -xq QuickImport; then exit 0; fi; "
        "for i in 1 2 3 4 5 6 7 8 9 10; do "
        "if mount | grep -E '\\((exfat|msdos)' | grep -vq read-only; then "
        "exec /usr/bin/open -a \"$0\" --args %1; fi; sleep 0.5; done")
        .arg(kArgument);
}

QString plistContents(const QString &bundle)
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "\t<key>Label</key>\n"
        "\t<string>%1</string>\n"
        "\t<key>ProgramArguments</key>\n"
        "\t<array>\n"
        "\t\t<string>/bin/sh</string>\n"
        "\t\t<string>-c</string>\n"
        "\t\t<string>%3</string>\n"
        "\t\t<string>%2</string>\n"
        "\t</array>\n"
        "\t<key>WatchPaths</key>\n"
        "\t<array>\n"
        "\t\t<string>/Volumes</string>\n"
        "\t</array>\n"
        "\t<key>RunAtLoad</key>\n"
        "\t<false/>\n"
        "</dict>\n"
        "</plist>\n")
        .arg(kLabel, xmlEscape(bundle), xmlEscape(agentScript()));
}

QString domain()
{
    return QStringLiteral("gui/%1").arg(getuid());
}

bool runLaunchctl(const QStringList &args, QString *error)
{
    QProcess proc;
    proc.start(QStringLiteral("/bin/launchctl"), args);
    if (!proc.waitForFinished(10000)) {
        if (error) *error = QStringLiteral("launchctl did not finish");
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        const QString out = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        qDebug() << "launchctl" << args << "failed:" << proc.exitCode() << out;
        if (error) *error = out.isEmpty()
                ? QStringLiteral("launchctl exited with code %1").arg(proc.exitCode())
                : out;
        return false;
    }
    return true;
}

// Unload the agent if launchd has it; failure is fine (it may not be loaded).
void bootout()
{
    runLaunchctl({QStringLiteral("bootout"), domain() + QLatin1Char('/') + kLabel}, nullptr);
}

bool install(const QString &bundle, QString *error)
{
    const QString path = plistPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) *error = QStringLiteral("Cannot create ~/Library/LaunchAgents");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(plistContents(bundle).toUtf8());
    file.close();
    // launchd refuses to bootstrap a label that is already loaded, so
    // always unload first.
    bootout();
    if (!runLaunchctl({QStringLiteral("bootstrap"), domain(), path}, error)) {
        QFile::remove(path);
        return false;
    }
    return true;
}

bool uninstall(QString *error)
{
    bootout();
    const QString path = plistPath();
    if (QFile::exists(path) && !QFile::remove(path)) {
        if (error) *error = QStringLiteral("Cannot remove %1").arg(path);
        return false;
    }
    return true;
}
#endif // Q_OS_MACOS

} // namespace

namespace CardAutostart {

bool isSupported()
{
#ifdef Q_OS_MACOS
    return !bundlePath().isEmpty();
#else
    return false;
#endif
}

bool isEnabled()
{
#ifdef Q_OS_MACOS
    return QFile::exists(plistPath());
#else
    return false;
#endif
}

bool setEnabled(bool enabled, QString *errorMessage)
{
#ifdef Q_OS_MACOS
    if (!enabled)
        return uninstall(errorMessage);
    const QString bundle = bundlePath();
    if (bundle.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("QuickImport is not running from an app bundle");
        return false;
    }
    return install(bundle, errorMessage);
#else
    Q_UNUSED(enabled);
    if (errorMessage) *errorMessage = QStringLiteral("Not supported on this platform");
    return false;
#endif
}

void refresh()
{
#ifdef Q_OS_MACOS
    if (!isEnabled())
        return;
    const QString bundle = bundlePath();
    if (bundle.isEmpty())
        return;
    QFile file(plistPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const QString current = QString::fromUtf8(file.readAll());
    file.close();
    if (current == plistContents(bundle))
        return;
    qDebug() << "Card autostart agent points elsewhere; rewriting for" << bundle;
    QString error;
    if (!install(bundle, &error))
        qWarning() << "Could not refresh card autostart agent:" << error;
#endif
}

QString launchArgument()
{
    return kArgument;
}

#ifdef Q_OS_MACOS
// IOKit registry entry id of the block device behind /dev/diskNsM. Every
// hotplug creates a new IOMedia entry with a new id, whereas the disk
// number is reused as soon as it is free. 0 when the device is not found.
static quint64 registryEntryId(const QByteArray &device)
{
    QByteArray bsdName = device;
    if (bsdName.startsWith("/dev/"))
        bsdName.remove(0, 5);
    CFMutableDictionaryRef match = IOBSDNameMatching(kIOMainPortDefault, 0, bsdName.constData());
    if (!match)
        return 0;
    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, match); // consumes match
    if (!service)
        return 0;
    quint64 id = 0;
    IORegistryEntryGetRegistryEntryID(service, &id);
    IOObjectRelease(service);
    return id;
}
#endif

// One insertion of one card. The device node and mount point alone are not
// enough: the disk number is reused by the next card, and nothing runs at
// removal time to prune the record (the agent only starts the app when a
// card is mounted). The IOKit registry id makes the key unique per hotplug.
static QString handledKey(const QStorageInfo &card)
{
    QString key = QString::fromUtf8(card.device()) + QLatin1Char('|') + card.rootPath();
#ifdef Q_OS_MACOS
    key += QLatin1Char('|') + QString::number(registryEntryId(card.device()));
#endif
    return key;
}

// Keep the setting bounded; entries for long-gone cards are harmless.
static const int kMaxHandled = 20;

static const QString kHandledSetting = QStringLiteral("autostartHandledCards");

bool wasHandled(const QStorageInfo &card)
{
    return QSettings().value(kHandledSetting).toStringList().contains(handledKey(card));
}

void markHandled(const QStorageInfo &card)
{
    if (!card.isValid())
        return;
    QSettings settings;
    QStringList handled = settings.value(kHandledSetting).toStringList();
    const QString key = handledKey(card);
    if (handled.contains(key))
        return;
    handled.append(key);
    while (handled.size() > kMaxHandled)
        handled.removeFirst();
    settings.setValue(kHandledSetting, handled);
    // Write it out now: QSettings otherwise flushes lazily, and a process
    // that is killed rather than quit would lose the record.
    settings.sync();
}

void pruneHandled(const QList<QStorageInfo> &mountedCards)
{
    QSettings settings;
    const QStringList handled = settings.value(kHandledSetting).toStringList();
    QStringList kept;
    for (const QString &key : handled) {
        for (const QStorageInfo &card : mountedCards) {
            if (handledKey(card) == key) {
                kept.append(key);
                break;
            }
        }
    }
    if (kept != handled) {
        settings.setValue(kHandledSetting, kept);
        settings.sync();
    }
}

} // namespace CardAutostart
