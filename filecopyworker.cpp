// CopyWorker.cpp
#include "filecopyworker.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QMap>
#include <QRegularExpression>
#include <QUuid>

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#endif

// Force file contents to physical storage. Used before deleting the source:
// a successful copy that only lives in the OS write cache is not a backup.
static bool flushFileToDisk(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const int fd = f.handle();
    if (fd < 0)
        return false;
#if defined(Q_OS_DARWIN)
    // F_FULLFSYNC asks the drive itself to flush; fall back to fsync.
    return ::fcntl(fd, F_FULLFSYNC) != -1 || ::fsync(fd) == 0;
#elif defined(Q_OS_UNIX)
    return ::fsync(fd) == 0;
#elif defined(Q_OS_WIN)
    return ::_commit(fd) == 0;
#else
    return true;
#endif
}

fileCopyWorker::fileCopyWorker(std::shared_ptr<fileCopyQueue> queue,
                               const QString &importFolder,
                               const QString &projectName,
                               const QString &fileNameFormat,
                               const bool &md5Check,
                               const bool &deleteAfterImport,
                               const bool &deleteExisting,
                               const QString &importBackupLocation)
    : queue(std::move(queue))
    , importBackupLocation(importBackupLocation)
    , importFolder(importFolder)
    , projectName(projectName)
    , fileNameFormat(fileNameFormat)
    , md5Check(md5Check)
    , deleteAfterImport(deleteAfterImport)
    , deleteExisting(deleteExisting)
{}

void fileCopyWorker::cancel()
{
    m_cancelRequested.store(true, std::memory_order_relaxed);
}

bool fileCopyWorker::wasCancelled() const
{
    return m_cancelRequested.load(std::memory_order_relaxed);
}

static QString replaceTokens(QString input, const QMap<QString, QString>& tokens)
{
    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it) {
        input.replace(it.key(), it.value());
    }
    return input;
}

QDateTime fileCopyWorker::captureTimestamp(const QFileInfo &info,
                                           const imageInfoStruct &imageInfo)
{
    return imageInfo.dateTimeOriginal.isValid() ? imageInfo.dateTimeOriginal
                                                : info.lastModified();
}

/*
 * Return: List: 0 = file, 1 = directory, 2 = total path
 */
QList<QString> fileCopyWorker::processNewFileName(QString importFolder,
                                                  QString projectName,
                                                  QDateTime lastModified,
                                                  imageInfoStruct imageInfo,
                                                  QFileInfo info,
                                                  QString fileNameFormat)

{
    QString project = projectName;

    // Prepare common time/camera tokens once
    const QDateTime& ts = lastModified;
    const QString weekStr = QString("%1").arg(ts.date().weekNumber(), 2, 10, QLatin1Char('0'));

    QMap<QString, QString> baseTokens{
        {"{D}", ts.toString("dd")},
        {"{m}", ts.toString("MM")},    // month (fixed: was using minutes)
        {"{y}", ts.toString("yy")},
        {"{Y}", ts.toString("yyyy")},
        {"{W}", weekStr},
        {"{h}", ts.toString("h")},
        {"{H}", ts.toString("hh")},
        {"{M}", ts.toString("mm")},    // minutes
        {"{i}", QString::number(imageInfo.isoValue)},
        {"{c}", QString::fromUtf8(imageInfo.serialNumber.toUtf8())},
        {"{T}", QString::fromUtf8(imageInfo.cameraName.toUtf8())},
        {"{O}", QString::fromUtf8(imageInfo.ownerName.toUtf8())},
        {"{o}", info.fileName()}
    };

    // First, resolve tokens for the project part
    const QString projectResolved = replaceTokens(project, baseTokens);

    QRegularExpression regex("_(\\d+)\\.(\\w+)$");
    QRegularExpressionMatch match = regex.match(info.fileName());
    QString sequenceNumber;
    QString extension;

    if (match.hasMatch()) {
        sequenceNumber = match.captured(1);
        extension = match.captured(2);
    }

    QMap<QString, QString> fileTokens = baseTokens; // start with base
    fileTokens.insert("{r}", sequenceNumber);
    fileTokens.insert("{e}", extension.isEmpty() ? QString() : QString(".%1").arg(extension));
    fileTokens.insert("{J}", projectResolved); // allow {J} to embed resolved project

    const QString resolvedFileName = replaceTokens(fileNameFormat, fileTokens);

    QString file = resolvedFileName;
    QString dir = importFolder;

    const QString fullPath = QString("%1/%2").arg(importFolder, resolvedFileName);

    regex.setPattern("^(.*/)?([^/]+)?$");
    QRegularExpressionMatch match2 = regex.match(fullPath);

    if (match2.hasMatch()) {
        dir = match2.captured(1).isEmpty() ? importFolder : match2.captured(1);
        file = match2.captured(2);
    }

    QList<QString> ret;
    ret.append(file);
    ret.append(dir);
    ret.append(fullPath);

    return ret;
}

namespace {
// One destination (import or backup) of a tee-copy.
struct CopyTarget
{
    QString finalPath;
    QString dirPath;
    bool existed = false;
    bool blocked = false;        // existed and may not be replaced
    bool alreadyMatched = false; // blocked, but identical content (MD5)
    bool copied = false;         // temp written, verified and renamed
    QString tempPath;
    std::unique_ptr<QFile> temp;

    bool succeeded() const { return copied || alreadyMatched; }
};
}

void fileCopyWorker::copyImages()
{
    int del = 0;
    int done = 0;

    auto calcMd5 = [](const QString &path) -> QByteArray {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return QByteArray();
        QCryptographicHash hash(QCryptographicHash::Md5);
        const qsizetype chunkSize = 16 * 1024 * 1024;
        while (!f.atEnd()) {
            const QByteArray chunk = f.read(chunkSize);
            hash.addData(chunk);
        }
        return hash.result();
    };

    auto makeTempPath = [](const QString &finalPath) {
        return QString("%1.quickimport.%2.part")
            .arg(finalPath, QUuid::createUuid().toString(QUuid::WithoutBraces));
    };

    // Read the source in chunks: allows cancelling mid-file, byte-level
    // progress, hashing while copying (no second read of the card for MD5)
    // and writing import + backup in one pass (tee).
    constexpr qint64 kChunkSize = 8 * 1024 * 1024;

    fileInfoStruct file;
    while (!wasCancelled() && queue->next(file)) {
        const QString sourcePath = file.fileInfo.filePath();
        const qint64 sourceSize = file.fileInfo.size();
        qint64 accounted = 0;

        QList<QString> fileTodo = fileCopyWorker::processNewFileName(importFolder,
                                                                     projectName,
                                                                     fileCopyWorker::captureTimestamp(file.fileInfo, file.imageInfo),
                                                                     file.imageInfo,
                                                                     file.fileInfo,
                                                                     fileNameFormat);
        CopyTarget dest;
        dest.finalPath = fileTodo[2];
        dest.dirPath = fileTodo[1];

        const bool doBackup = !importBackupLocation.isEmpty();
        CopyTarget backup;
        if (doBackup) {
            const QList<QString> backupTodo
                = fileCopyWorker::processNewFileName(importBackupLocation,
                                                     projectName,
                                                     fileCopyWorker::captureTimestamp(file.fileInfo, file.imageInfo),
                                                     file.imageInfo,
                                                     file.fileInfo,
                                                     fileNameFormat);
            backup.finalPath = backupTodo[2];
            backup.dirPath = backupTodo[1];
        }

        CopyTarget *targets[2] = {&dest, doBackup ? &backup : nullptr};

        qDebug() << "Copy" << sourcePath << dest.finalPath;

        int activeCount = 0;
        for (CopyTarget *t : targets) {
            if (!t)
                continue;
            if (!QDir().exists(t->dirPath))
                QDir().mkpath(t->dirPath);
            t->existed = QFile::exists(t->finalPath);
            t->blocked = t->existed && !deleteExisting;
            if (!t->blocked)
                ++activeCount;
        }

        QByteArray sourceMd5;
        bool cancelled = false;

        if (activeCount > 0) {
            bool ioFailed = false;

            QFile source(sourcePath);
            if (!source.open(QIODevice::ReadOnly)) {
                ioFailed = true;
            } else {
                for (CopyTarget *t : targets) {
                    if (!t || t->blocked)
                        continue;
                    t->tempPath = makeTempPath(t->finalPath);
                    QFile::remove(t->tempPath);
                    t->temp.reset(new QFile(t->tempPath));
                    if (!t->temp->open(QIODevice::WriteOnly))
                        ioFailed = true;
                }

                QCryptographicHash hash(QCryptographicHash::Md5);
                while (!ioFailed && !source.atEnd()) {
                    if (wasCancelled()) {
                        cancelled = true;
                        break;
                    }
                    const QByteArray chunk = source.read(kChunkSize);
                    if (chunk.isEmpty()) {
                        if (!source.atEnd())
                            ioFailed = true;
                        break;
                    }
                    if (md5Check)
                        hash.addData(chunk);
                    for (CopyTarget *t : targets) {
                        if (!t || t->blocked)
                            continue;
                        if (t->temp->write(chunk) != chunk.size()) {
                            ioFailed = true;
                            break;
                        }
                    }
                    accounted += chunk.size();
                    emit bytesAccounted(chunk.size());
                }

                for (CopyTarget *t : targets) {
                    if (t && t->temp)
                        t->temp->close();
                }

                if (!ioFailed && !cancelled && md5Check)
                    sourceMd5 = hash.result();
            }

            if (ioFailed || cancelled) {
                for (CopyTarget *t : targets) {
                    if (t && !t->tempPath.isEmpty())
                        QFile::remove(t->tempPath);
                }
            } else {
                // Verify each temp on its own disk, then commit atomically
                for (CopyTarget *t : targets) {
                    if (!t || t->blocked)
                        continue;
                    bool good = QFileInfo(t->tempPath).size() == sourceSize;
                    if (good && md5Check)
                        good = !sourceMd5.isEmpty() && calcMd5(t->tempPath) == sourceMd5;
                    // When the source will be deleted afterwards, make sure
                    // the copy is physically on disk before committing it.
                    if (good && deleteAfterImport)
                        good = flushFileToDisk(t->tempPath);
                    if (good && t->existed)
                        good = QFile::remove(t->finalPath);
                    if (good)
                        good = QFile::rename(t->tempPath, t->finalPath);
                    if (good)
                        t->copied = true;
                    else
                        QFile::remove(t->tempPath);
                }
            }
        }

        if (cancelled)
            break; // file not counted; temps are already cleaned up

        // A blocked target still counts as success when MD5 shows the
        // existing file is identical to the source.
        if (md5Check) {
            for (CopyTarget *t : targets) {
                if (!t || !t->blocked)
                    continue;
                if (sourceMd5.isEmpty())
                    sourceMd5 = calcMd5(sourcePath);
                if (!sourceMd5.isEmpty() && calcMd5(t->finalPath) == sourceMd5)
                    t->alreadyMatched = true;
            }
        }

        const bool ok = dest.succeeded() && (!doBackup || backup.succeeded());
        if (ok)
            cnt++;
        else
            fail++;

        if (!ok) {
            QString message;
            if (dest.blocked && !dest.alreadyMatched) {
                message = tr("Destination already exists for %1: %2")
                              .arg(file.fileInfo.fileName())
                              .arg(dest.finalPath);
            } else if (doBackup && !backup.succeeded()) {
                message = tr("Backup failed for %1 -> %2")
                              .arg(file.fileInfo.fileName())
                              .arg(backup.finalPath);
            } else {
                message = tr("Copy failed for %1 -> %2")
                              .arg(file.fileInfo.fileName())
                              .arg(dest.finalPath);
            }
            emit errorOccurred(message);
        }

        if (ok && deleteAfterImport) {
            QFile sourceFile(sourcePath);
            if (sourceFile.remove()) {
                del++;
            } else {
                emit errorOccurred(tr("Failed to delete source file %1").arg(sourcePath));
            }
        }

        done = done + 1;
        // Account skipped/failed remainder so the byte-based progress bar
        // still reaches 100%
        if (accounted < sourceSize)
            emit bytesAccounted(sourceSize - accounted);
        emit progressUpdated(0, done, cnt, fail, del);
        emit fileProcessed(ok && activeCount > 0 ? sourceSize : 0);
        emit lastLocationImportedTo(fileTodo[1]);
    }

    emit copyingFinished();
}
