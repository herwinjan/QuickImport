// CopyWorker.h
#ifndef COPYWORKER_H
#define COPYWORKER_H

#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

class fileCopyWorker : public QObject
{
    Q_OBJECT
public:
    fileCopyWorker(const QList<QFileInfo> &list,
                   const QString &importFolder,
                   const QString &projectName,
                   const bool &md5Check,
                   const bool &deleteAfterImport,
                   const bool &deleteExisting);
    void cancel();
    bool doCancel = false;
public slots:
    void copyImages();
signals:
    void progressUpdated(int, int, int, int, int);
    void copyingFinished();

private:
    int cnt = 0, fail = 0;

    QList<QFileInfo> list;
    QString importFolder;
    QString projectName;
    bool md5Check;
    bool deleteAfterImport;
    bool deleteExisting;
    QTimer *timer;
};

#endif // COPYWORKER_H
