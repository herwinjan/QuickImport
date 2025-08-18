// CopyWorker.cpp
#include "filecopyworker.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QRegularExpression>
#include <QMap>

fileCopyWorker::fileCopyWorker(const QList<fileInfoStruct> &list,
                               const QString &importFolder,
                               const QString &projectName,
                               const QString &fileNameFormat,
                               const bool &md5Check,
                               const bool &deleteAfterImport,
                               const bool &deleteExisting,
                               const QString &importBackupLocation)
    : list(list)
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
    doCancel = true;
}

static QString replaceTokens(QString input, const QMap<QString, QString>& tokens)
{
    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it) {
        input.replace(it.key(), it.value());
    }
    return input;
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

void fileCopyWorker::copyImages()
{
    int del = 0;

    int totalFiles = list.size();
    int done = 0;

    foreach (const fileInfoStruct &file, list) {
        if (doCancel) {
            emit copyingFinished();
            qDebug() << "stop copieing";
            return;
        }

        QList<QString> fileTodo = fileCopyWorker::processNewFileName(importFolder,
                                                                     projectName,
                                                                     file.fileInfo.lastModified(),
                                                                     file.imageInfo,
                                                                     file.fileInfo,
                                                                     fileNameFormat);

        QList<QString> backupTodo;
        bool doBackup = false;
        QString newBackupFile;

        if (!importBackupLocation.isEmpty()) {
            doBackup = true;
            backupTodo = fileCopyWorker::processNewFileName(importBackupLocation,
                                                            projectName,
                                                            file.fileInfo.lastModified(),
                                                            file.imageInfo,
                                                            file.fileInfo,
                                                            fileNameFormat);
            newBackupFile = backupTodo[2];

            // create full backup path (mkpath creates intermediate directories)
            if (!QDir().exists(backupTodo[1]))
                QDir().mkpath(backupTodo[1]);
        }

        QString newFile = fileTodo[2];



        // ensure destination directory exists (use mkpath)
        if (!QDir().exists(fileTodo[1]))
            QDir().mkpath(fileTodo[1]);

        qDebug() << "Copy " << file.fileInfo.filePath() << newFile;
        qDebug() << fileTodo;
        qDebug() << fileTodo[0] << fileTodo[1] << fileTodo[2];

        bool destExisted = QFile::exists(newFile);
        bool ok = false;
        // If we are going to delete the source anyway and no backup is required,
        // try a fast rename (same-filesystem move). This is much faster than copy.
        if (deleteAfterImport && !doBackup) {
            ok = QFile::rename(file.fileInfo.filePath(), newFile);
        }
        if (!ok) {
            ok = QFile::copy(file.fileInfo.filePath(), newFile);
        }
        bool backupOK = false;

        if (doBackup) {
            backupOK = QFile::copy(file.fileInfo.filePath(), newBackupFile);
        }

        if (ok) {
            if (doBackup && backupOK != true) {
                ok = false;
                fail++;
            } else {
                cnt++;
            }
        } else
            fail++;
        QFileInfo newFileInfo(newFile);

        bool goDelete = true;
        if (!ok) {
            goDelete = false;
            if (destExisted && deleteExisting) {
                if (newFileInfo.size() == file.fileInfo.size()
                    && newFileInfo.birthTime() == file.fileInfo.birthTime()) {
                    goDelete = true;
                }
            }
        }
        QFile sourceFile(file.fileInfo.filePath());

        if (md5Check && ok) {
            // Stream MD5 to avoid loading entire files into memory
            auto calcMd5 = [](const QString &path)->QByteArray {
                QFile f(path);
                if (!f.open(QIODevice::ReadOnly))
                    return QByteArray();
                QCryptographicHash hash(QCryptographicHash::Md5);
                const qsizetype chunkSize = 16 * 1024 * 1024; // 16MB
                while (!f.atEnd()) {
                    QByteArray chunk = f.read(chunkSize);
                    hash.addData(chunk);
                }
                return hash.result();
            };

            QByteArray sourceMd5 = calcMd5(file.fileInfo.filePath());
            QByteArray destinationMd5 = calcMd5(newFileInfo.filePath());

            // Compare MD5 hashes
            if (sourceMd5.isEmpty() || destinationMd5.isEmpty() || sourceMd5 != destinationMd5) {
               // QMessageBox::critical(nullptr,
               //                       tr("Error"),
               //                       tr("MD5 check failed (files are different)."));
                emit errorOccurred(tr("MD5 check failed for %1 -> %2")
                                  .arg(file.fileInfo.fileName())
                                  .arg(newFile));
                qWarning() << "MD5 check failed for" << file.fileInfo.filePath() << "->" << newFile;
                goDelete = false;

            }
        }
        if (file.fileInfo.size() != newFileInfo.size()) {
            qDebug() << "Do not Delete!! Size differs";
            goDelete = false;
        }
        qDebug() << goDelete;
        // ui->statusbar->showMessage(QString("copying, %1 files copied.").arg(cnt));

        if (goDelete && deleteAfterImport) {
            sourceFile.remove();
            del++;
        }
        //TODOui->status->setText(
        //QString("Copy file %1 of %2. %3").arg(cnt).arg(list.count()).arg(err));
        done = done + 1;
        emit progressUpdated((done * 100) / totalFiles, done, cnt, fail, del);
        emit lastLocationImportedTo(fileTodo[1]);
        
    }

    emit copyingFinished();
}
