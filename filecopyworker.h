// CopyWorker.h
#ifndef COPYWORKER_H
#define COPYWORKER_H

#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include "filelistmodel.h"

class fileCopyWorker : public QObject
{
    Q_OBJECT
public:
    fileCopyWorker(const QList<fileInfoStruct> &list,
                   const QString &importFolder,
                   const QString &projectName,
                   const QString &fileNameFormat,
                   const bool &md5Check,
                   const bool &deleteAfterImport,
                   const bool &deleteExisting,
                   const QString &importBackupLocation);
    void cancel();
    bool doCancel = false;
    static QList<QString> processNewFileName(
        QString, QString, QDateTime, imageInfoStruct, QFileInfo, QString);
public slots:
    void copyImages();
signals:
    void progressUpdated(int, int, int, int, int);
    void copyingFinished();

private:
    int cnt = 0, fail = 0;

    QList<fileInfoStruct> list;
    QString importBackupLocation;
    QString importFolder;
    QString projectName;
    QString fileNameFormat;
    bool md5Check;
    bool deleteAfterImport;
    bool deleteExisting;
    QTimer *timer;
};

#endif // COPYWORKER_H
