#ifndef FILECOPYDIALOG_H
#define FILECOPYDIALOG_H

#include "filecopyworker.h"

#include <QDialog>
#include <QFileInfo>

namespace Ui {
class fileCopyDialog;
}

class fileCopyDialog : public QDialog
{
    Q_OBJECT

public:
    fileCopyDialog(const QList<fileInfoStruct> &list,
                   const QString &importFolder,
                   const QString &projectName,
                   const QString &fileNameFormat,
                   const bool &md5Check,
                   const bool &deleteAfterImport,
                   const bool &deleteExisting,
                   const QString &importBackupFolder,
                   QWidget *parent = nullptr);
    ~fileCopyDialog();

    QString getLastFilePath();
private slots:
    void on_cancelButton_clicked();

    void handleProgress(int progress, int, int, int, int);
    void handleFinished();

    void lastLocationImportedToSlot(QString);

private:
    Ui::fileCopyDialog *ui;
    int count = 0;
    QString lastFilePath;

    QThread *m_thread;
    fileCopyWorker *m_worker;

    // --- Parallel copy support (two workers) ---
    void handleProgressFromWorker(int workerIndex, int progress, int done, int cnt, int fail, int del);
    void handleWorkerFinished();

    // Aggregated state across workers
    int m_totalFiles = 0;
    int m_workerCount = 1;
    int m_finishedWorkers = 0;
    // Per-worker tallies
    int m_done[2] = {0,0};
    int m_cnt[2]  = {0,0};
    int m_fail[2] = {0,0};
    int m_del[2]  = {0,0};

    QThread *m_thread2 = nullptr;
    fileCopyWorker *m_worker2 = nullptr;

};

#endif // FILECOPYDIALOG_H
