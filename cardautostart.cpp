#include "cardautostart.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#ifdef Q_OS_MACOS
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
        "\t\t<string>/usr/bin/open</string>\n"
        "\t\t<string>-a</string>\n"
        "\t\t<string>%2</string>\n"
        "\t\t<string>--args</string>\n"
        "\t\t<string>%3</string>\n"
        "\t</array>\n"
        "\t<key>WatchPaths</key>\n"
        "\t<array>\n"
        "\t\t<string>/Volumes</string>\n"
        "\t</array>\n"
        "\t<key>RunAtLoad</key>\n"
        "\t<false/>\n"
        "</dict>\n"
        "</plist>\n")
        .arg(kLabel, xmlEscape(bundle), kArgument);
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

} // namespace CardAutostart
