#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QStringList>

// Persistent preview loader. Lives on one worker thread for the lifetime of
// the main window (no thread churn per preview). Requests are coalesced
// "latest wins", decoded previews go into an LRU cache, and neighbours of
// the current selection can be prefetched for instant browsing.
class imageLoader : public QObject
{
    Q_OBJECT
public:
    explicit imageLoader(QObject *parent = nullptr);

    // Thread-safe; call from the GUI thread. The result arrives via
    // imageLoaded() with the same path, so stale results can be dropped.
    void requestImage(const QString &path);
    // Thread-safe; replaces the current prefetch list (processed when idle).
    void requestPrefetch(const QStringList &paths);

public slots:
    // Queued onto the loader thread; call when the card is reloaded so a
    // re-inserted (modified) card cannot serve stale previews.
    void clearCache();

signals:
    void imageLoaded(const QString &path, const QImage &image, bool failed);

private slots:
    void processRequests();

private:
    QImage loadFromDisk(const QString &path, bool *failed);
    void scheduleWake(); // caller must hold m_mutex

    QMutex m_mutex;
    QString m_pending;      // latest main request (latest wins)
    QStringList m_prefetch; // pending prefetch paths
    bool m_wakeQueued = false;

    // Only touched on the loader thread; cost unit = KB
    QCache<QString, QImage> m_cache;
};

#endif // IMAGELOADER_H
