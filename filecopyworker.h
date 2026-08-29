// CopyWorker.h
#ifndef COPYWORKER_H
#define COPYWORKER_H

#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <atomic>
#include <memory>
#include "fileinfomodel.h"

// Thread-safe work queue shared by the copy workers. The file list is
// immutable; workers claim the next file with an atomic index, so any number
// of workers can pull from the same queue without locking.
class fileCopyQueue
{
public:
    explicit fileCopyQueue(const QList<fileInfoStruct> &files)
        : m_files(files)
    {}

    bool next(fileInfoStruct &out)
    {
        const int i = m_next.fetch_add(1, std::memory_order_relaxed);
        if (i >= m_files.size())
            return false;
        out = m_files.at(i);
        return true;
    }

    int remaining() const
    {
        return qMax(0, int(m_files.size()) - m_next.load(std::memory_order_relaxed));
    }

    int total() const { return int(m_files.size()); }

private:
    const QList<fileInfoStruct> m_files;
    std::atomic_int m_next{0};
};

class fileCopyWorker : public QObject
{
    Q_OBJECT
public:
    fileCopyWorker(std::shared_ptr<fileCopyQueue> queue,
                   const QString &importFolder,
                   const QString &projectName,
                   const QString &fileNameFormat,
                   const bool &md5Check,
                   const bool &deleteAfterImport,
                   const bool &deleteExisting,
                   const QString &importBackupLocation);
    void cancel();
    bool wasCancelled() const;
    static QList<QString> processNewFileName(
        QString, QString, QDateTime, imageInfoStruct, QFileInfo, QString);
    // Timestamp used for grouping and naming tokens: EXIF DateTimeOriginal
    // when available, otherwise the file's modification time.
    static QDateTime captureTimestamp(const QFileInfo &info,
                                      const imageInfoStruct &imageInfo);
public slots:
    void copyImages();
signals:
    void progressUpdated(int, int, int, int, int);
    void copyingFinished();
    void lastLocationImportedTo(QString);
    void errorOccurred(const QString &message);
    // Emitted after each file with the number of source bytes successfully
    // copied (0 on failure); the dialog uses this to measure throughput.
    void fileProcessed(qint64 copiedBytes);
    // Emitted per copied chunk (and per-file remainder) so the dialog can
    // show byte-level progress; the deltas sum to the total source bytes.
    void bytesAccounted(qint64 delta);

private:
    int cnt = 0, fail = 0;

    std::shared_ptr<fileCopyQueue> queue;
    QString importBackupLocation;
    QString importFolder;
    QString projectName;
    QString fileNameFormat;
    bool md5Check;
    bool deleteAfterImport;
    bool deleteExisting;
    QTimer *timer;
    std::atomic_bool m_cancelRequested{false};
};

#endif // COPYWORKER_H
