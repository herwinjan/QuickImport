// CopyWorker.cpp
#include "filecopyworker.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QMessageBox>

fileCopyWorker::fileCopyWorker(const QList<fileInfoStruct> &list,
                               const QString &importFolder,
                               const QString &projectName,
                               const QString &fileNameFormat,
                               const bool &md5Check,
                               const bool &deleteAfterImport,
                               const bool &deleteExisting,
                               const QString &importBackupLocation)
    : list(list)
    , importFolder(importFolder)
    , projectName(projectName)
    , fileNameFormat(fileNameFormat)
    , md5Check(md5Check)
    , deleteAfterImport(deleteAfterImport)
    , deleteExisting(deleteExisting)
    , importBackupLocation(importBackupLocation)
{}

void fileCopyWorker::cancel()
{
    doCancel = true;
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

    project.replace("{D}", lastModified.toString("dd"));
    project.replace("{m}", lastModified.toString("mm"));
    project.replace("{y}", lastModified.toString("yy"));
    project.replace("{Y}", lastModified.toString("yyyy"));

    project.replace("{W}", QString("%1").arg(lastModified.date().weekNumber()));
    project.replace("{h}", lastModified.toString("h"));
    project.replace("{H}", lastModified.toString("hh"));
    project.replace("{M}", lastModified.toString("mm"));

    project.replace("{i}", QString("%1").arg(imageInfo.isoValue));
    project.replace("{c}", QString("%1").arg(imageInfo.serialNumber));
    project.replace("{T}", QString("%1").arg(imageInfo.cameraName));
    project.replace("{O}", QString("%1").arg(imageInfo.ownerName));

    //File name

    fileNameFormat.replace("{D}", lastModified.toString("dd"));
    fileNameFormat.replace("{m}", lastModified.toString("MM"));
    fileNameFormat.replace("{y}", lastModified.toString("yy"));
    fileNameFormat.replace("{Y}", lastModified.toString("yyyy"));

    fileNameFormat.replace("{W}", QString("%1").arg(lastModified.date().weekNumber()));
    fileNameFormat.replace("{h}", lastModified.toString("h"));
    fileNameFormat.replace("{H}", lastModified.toString("hh"));
    fileNameFormat.replace("{M}", lastModified.toString("mm"));

    fileNameFormat.replace("{i}", QString("%1").arg(imageInfo.isoValue));
    fileNameFormat.replace("{c}", QString("%1").arg(imageInfo.serialNumber));
    fileNameFormat.replace("{T}", QString("%1").arg(imageInfo.cameraName));
    fileNameFormat.replace("{O}", QString("%1").arg(imageInfo.ownerName));
    fileNameFormat.replace("{o}", QString("%1").arg(info.fileName()));
    fileNameFormat.replace("{J}", QString("%1").arg(project));

    QRegularExpression regex;
    regex.setPattern("_(\\d+)\\.(\\w+)$");
    QRegularExpressionMatch match = regex.match(info.fileName());
    QString sequenceNumber;
    QString extension;

    if (match.hasMatch()) {
        sequenceNumber = match.captured(1);
        extension = match.captured(2);
    }
    fileNameFormat.replace("{e}", QString(".%1").arg(extension));
    fileNameFormat.replace("{r}", QString("%1").arg(sequenceNumber));
    QString file = fileNameFormat;
    QString dir = importFolder;

    QString fullPath = QString("%1/%2").arg(importFolder, fileNameFormat);

    regex.setPattern("^(.*/)?([^/]+)?$");
    QRegularExpressionMatch match2 = regex.match(fullPath);

    if (match2.hasMatch()) {
        dir = match2.captured(1).isEmpty() ? "Root or same directory" : match2.captured(1);
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

            QDir dirBackup(backupTodo[1]);

            if (!dirBackup.exists())
                dirBackup.mkdir(backupTodo[1]);
        }

        QString newFile = fileTodo[2];
        QDir dir(fileTodo[1]);
        qDebug() << "Copy " << file.fileInfo.filePath() << newFile;
        qDebug() << fileTodo;
        qDebug() << fileTodo[0] << fileTodo[1] << fileTodo[2];

        if (!dir.exists())
            dir.mkdir(fileTodo[1]);

        bool exist = QFile::exists(file.fileInfo.filePath());
        bool ok = QFile::copy(file.fileInfo.filePath(), newFile);
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
            if (exist && deleteExisting) {
                if (newFileInfo.size() == file.fileInfo.size()
                    && newFileInfo.birthTime() == file.fileInfo.birthTime()) {
                    goDelete = true;
                }
            }
        }
        QFile sourceFile(file.fileInfo.filePath());

        if (md5Check && ok) {
            QFile sourceFile(file.fileInfo.filePath());
            QFile destinationFile(newFileInfo.filePath());

            // Calculate MD5 hash of source file
            sourceFile.open(QIODevice::ReadOnly);
            QByteArray sourceData = sourceFile.readAll();
            sourceFile.close();
            QByteArray sourceMd5 = QCryptographicHash::hash(sourceData, QCryptographicHash::Md5);

            // Calculate MD5 hash of destination file
            destinationFile.open(QIODevice::ReadOnly);
            QByteArray destinationData = destinationFile.readAll();
            destinationFile.close();
            QByteArray destinationMd5 = QCryptographicHash::hash(destinationData,
                                                                 QCryptographicHash::Md5);

            // Compare MD5 hashes
            if (sourceMd5 != destinationMd5) {
                QMessageBox::critical(nullptr,
                                      tr("Error"),
                                      tr("MD5 check failed (files are different)."));

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
