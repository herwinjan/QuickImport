#ifndef FILECOPYDIALOG_H
#define FILECOPYDIALOG_H

#include "filecopyworker.h"

#include <QDialog>
#include <QElapsedTimer>
#include <QFileInfo>
#include <memory>

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
    // Number of files actually copied (or verified identical) — 0 when the
    // whole import failed or everything was skipped.
    int copiedCount() const { return m_cnt[0] + m_cnt[1]; }
private slots:
    void on_cancelButton_clicked();

    void handleProgress(int progress, int, int, int, int);
    void handleFinished();

    void lastLocationImportedToSlot(QString);

private:
    Ui::fileCopyDialog *ui;
    int count = 0;
    QString lastFilePath;

    QThread *m_thread = nullptr;
    fileCopyWorker *m_worker = nullptr;
    QThread *m_thread2 = nullptr;
    fileCopyWorker *m_worker2 = nullptr;

    // --- Adaptive parallel copy (shared queue, 1 or 2 workers) ---
    // All workers pull from one shared queue. We always start with a single
    // worker and measure the throughput over the first files; only a fast
    // reader (e.g. CFexpress) gets a second worker.
    void startWorker(int workerIndex);
    void handleFileProcessed(qint64 copiedBytes);
    void handleBytesAccounted(qint64 delta);
    void showErrorSummary();
    QStringList m_errors;
    void handleProgressFromWorker(int workerIndex, int progress, int done, int cnt, int fail, int del);
    void handleWorkerFinished();
    void handleThreadStopped(int workerIndex);
    void finalizeIfReady();

    std::shared_ptr<fileCopyQueue> m_queue;

    // Worker construction parameters (needed when the second worker is
    // started later, once the throughput is known)
    QString m_importFolder;
    QString m_projectName;
    QString m_fileNameFormat;
    bool m_md5Check = false;
    bool m_deleteAfterImport = false;
    bool m_deleteExisting = false;
    QString m_importBackupFolder;

    // Throughput probe
    QElapsedTimer m_copyTimer;
    qint64 m_bytesProcessed = 0;
    // Byte-level progress
    qint64 m_totalSourceBytes = 0;
    qint64 m_bytesAccounted = 0;
    bool m_secondWorkerDecided = false;
    bool m_allowSecondWorker = true;

    // Aggregated state across workers
    int m_totalFiles = 0;
    int m_workerCount = 1;
    int m_finishedWorkers = 0;
    int m_stoppedThreads = 0;
    bool m_cancelRequested = false;
    bool m_closeScheduled = false;
    // Per-worker tallies
    int m_done[2] = {0,0};
    int m_cnt[2]  = {0,0};
    int m_fail[2] = {0,0};
    int m_del[2]  = {0,0};
};

#endif // FILECOPYDIALOG_H
