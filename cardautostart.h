#ifndef CARDAUTOSTART_H
#define CARDAUTOSTART_H

#include <QList>
#include <QString>
#include <QStorageInfo>

// "Start QuickImport when a card is inserted".
//
// An app that is not running cannot notice a card, so this is delegated to
// launchd: a per-user LaunchAgent watches /Volumes and runs `open -a
// QuickImport.app --args --card-inserted` whenever a volume is mounted or
// unmounted. `open` activates the running instance instead of starting a
// second one. When started this way the app checks whether a memory card
// is actually mounted and quits silently otherwise (see main.cpp).
//
// The state lives in the LaunchAgent plist itself, not in QSettings, so the
// checkbox always shows what launchd will really do. macOS only; on other
// platforms isSupported() is false and the other calls are no-ops.
namespace CardAutostart {

bool isSupported();

// Whether the LaunchAgent is installed.
bool isEnabled();

// Install/remove the LaunchAgent for the running app bundle. Returns false
// (with a message in errorMessage) when writing the plist or talking to
// launchctl fails.
bool setEnabled(bool enabled, QString *errorMessage = nullptr);

// Called at start-up: if the agent is installed but points at a different
// bundle path (the app was moved or updated), rewrite it for this one.
void refresh();

// Command-line argument the agent starts the app with.
QString launchArgument();

// The agent also fires for writes inside mounted volumes, so a start for a
// card that is already on screen (or was, before the user quit) must not
// bring the window back. These remember the card mounts already shown:
// markHandled() when a card is loaded, pruneHandled() with the cards that
// are mounted right now (a removed card drops out, so re-inserting it counts
// as new), wasHandled() to decide whether to quit silently.
bool wasHandled(const QStorageInfo &card);
void markHandled(const QStorageInfo &card);
void pruneHandled(const QList<QStorageInfo> &mountedCards);

} // namespace CardAutostart

#endif // CARDAUTOSTART_H
