// CopyWorker.cpp
#include "filecopyworker.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QMessageBox>

fileCopyWorker::fileCopyWorker(const QList<QFileInfo> &list,
                               const QString &importFolder,
                               const QString &projectName,
                               const bool &md5Check,
                               const bool &deleteAfterImport)
    : list(list)
    , importFolder(importFolder)
    , projectName(projectName)
    , md5Check(md5Check)
    , deleteAfterImport(deleteAfterImport)
{}

void fileCopyWorker::cancel()
{
    doCancel = true;
}

void fileCopyWorker::copyImages()
{
    int del = 0;

    int totalFiles = list.size();
    int done = 0;

    foreach (const QFileInfo &file, list) {
        if (doCancel) {
            emit copyingFinished();
            qDebug() << "stop copieing";
            return;
        }
        QString newFile = QString("%1/%2/%3").arg(importFolder, projectName, file.fileName());
        qDebug() << "Copy " << file.filePath() << newFile;
        bool ok = QFile::copy(file.filePath(), newFile);

        qDebug() << ok;
        if (ok)
            cnt++;
        else
            fail++;
        QFileInfo newFileInfo(newFile);

        bool goDelete = true;
        QFile sourceFile(file.filePath());

        if (md5Check && ok) {
            QFile sourceFile(file.filePath());
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
        if (file.size() != newFileInfo.size()) {
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
        emit progressUpdated((done++ * 100) / totalFiles, done, cnt, fail, del);
    }

    emit copyingFinished();
}
