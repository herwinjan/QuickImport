#include "filecopydialog.h"
#include <QCryptographicHash>
#include <QHash>
#include <QMessageBox>
#include <QSet>
#include <QThread>
#include <QTimer>
#include "ui_filecopydialog.h"

fileCopyDialog::fileCopyDialog(const QList<fileInfoStruct> &list,
                               const QString &importFolder,
                               const QString &projectName,
                               const QString &fileNameFormat,
                               const bool &md5Check,
                               const bool &deleteAfterImport,
                               const bool &deleteExisting,
                               const QString &importBackupFolder,
                               QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::fileCopyDialog)
{
    ui->setupUi(this);
    ui->progressBar->setRange(0, 100);

    // Total file count across all workers
    count = list.count();
    m_totalFiles = count;

    ui->status->setText(QString(tr("Copy file %1 of %2.")).arg(0).arg(count));
    ui->progressBar->setValue(0);

    m_importFolder = importFolder;
    m_projectName = projectName;
    m_fileNameFormat = fileNameFormat;
    m_md5Check = md5Check;
    m_deleteAfterImport = deleteAfterImport;
    m_deleteExisting = deleteExisting;
    m_importBackupFolder = importBackupFolder;

    // With duplicate destination paths two workers could write to the same
    // target file concurrently, so stay single-threaded in that case.
    // Checking the import destinations also covers the backup destinations:
    // both use the same resolved file name, only the folder prefix differs.
    bool hasDuplicateDestinations = false;
    QSet<QString> seenDestinations;
    QHash<QString, QString> sourceByDestination;
    for (const fileInfoStruct &file : list) {
        const QList<QString> fileTodo = fileCopyWorker::processNewFileName(importFolder,
                                                                           projectName,
                                                                           fileCopyWorker::captureTimestamp(file.fileInfo, file.imageInfo),
                                                                           file.imageInfo,
                                                                           file.fileInfo,
                                                                           fileNameFormat);
        const QString destinationPath = fileTodo.value(2);
        if (seenDestinations.contains(destinationPath)) {
            hasDuplicateDestinations = true;
            qWarning() << "Duplicate import destination detected:" << destinationPath
                       << "for" << sourceByDestination.value(destinationPath)
                       << "and" << file.fileInfo.filePath();
            break;
        }
        seenDestinations.insert(destinationPath);
        sourceByDestination.insert(destinationPath, file.fileInfo.filePath());
    }

    m_allowSecondWorker = !hasDuplicateDestinations;

    // All workers share one queue. Start with a single worker; a second one
    // is only started from handleFileProcessed() when the measured
    // throughput shows a fast reader (e.g. CFexpress instead of SD).
    m_totalSourceBytes = 0;
    for (const fileInfoStruct &file : list)
        m_totalSourceBytes += file.fileInfo.size();

    m_queue = std::make_shared<fileCopyQueue>(list);
    m_workerCount = 1;
    m_copyTimer.start();
    startWorker(0);
}

void fileCopyDialog::handleBytesAccounted(qint64 delta)
{
    m_bytesAccounted += delta;
    if (m_totalSourceBytes > 0) {
        const qint64 pct = (m_bytesAccounted * 100) / m_totalSourceBytes;
        const int progress = int(qMin<qint64>(qMax<qint64>(pct, 0), 100));
        ui->progressBar->setValue(progress);
    }
}

void fileCopyDialog::startWorker(int workerIndex)
{
    auto *thread = new QThread(this);
    auto *worker = new fileCopyWorker(m_queue,
                                      m_importFolder,
                                      m_projectName,
                                      m_fileNameFormat,
                                      m_md5Check,
                                      m_deleteAfterImport,
                                      m_deleteExisting,
                                      m_importBackupFolder);
    if (workerIndex == 0) {
        m_thread = thread;
        m_worker = worker;
    } else {
        m_thread2 = thread;
        m_worker2 = worker;
    }

    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &fileCopyWorker::copyImages);
    connect(worker, &fileCopyWorker::progressUpdated, this,
            [this, workerIndex](int progress, int done, int cnt, int fail, int del){
                handleProgressFromWorker(workerIndex, progress, done, cnt, fail, del);
            });
    connect(worker, &fileCopyWorker::fileProcessed,
            this, &fileCopyDialog::handleFileProcessed);
    connect(worker, &fileCopyWorker::bytesAccounted,
            this, &fileCopyDialog::handleBytesAccounted);
    connect(worker, &fileCopyWorker::lastLocationImportedTo,
            this, &fileCopyDialog::lastLocationImportedToSlot);
    connect(worker, &fileCopyWorker::errorOccurred, this, [this](const QString &msg){
               QMessageBox::critical(this, tr("Error"), msg);
           }, Qt::QueuedConnection);
    connect(worker, &fileCopyWorker::copyingFinished, this, &fileCopyDialog::handleWorkerFinished);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, workerIndex]() { handleThreadStopped(workerIndex); });
    thread->start();
}

void fileCopyDialog::handleFileProcessed(qint64 copiedBytes)
{
    m_bytesProcessed += copiedBytes;

    if (m_secondWorkerDecided || !m_allowSecondWorker)
        return;

    // Decide once, after enough data has flowed to give a stable measurement.
    constexpr qint64 kProbeBytes = 200LL * 1000 * 1000; // 200 MB
    constexpr double kMinCardReadMBps = 300.0;          // below this a 2nd worker hurts

    if (m_bytesProcessed < kProbeBytes)
        return;
    m_secondWorkerDecided = true;

    const double seconds = m_copyTimer.elapsed() / 1000.0;
    if (seconds <= 0)
        return;

    // The tee-copy reads the source exactly once (MD5 is hashed during the
    // copy), so copied bytes equal card reads.
    const double cardMBps = m_bytesProcessed / 1e6 / seconds;
    qDebug() << "Copy throughput probe:" << cardMBps << "MB/s (card read estimate)";

    if (cardMBps >= kMinCardReadMBps && m_queue->remaining() > 1
        && m_finishedWorkers == 0 && !m_cancelRequested) {
        qDebug() << "Fast reader detected, starting second copy worker";
        m_workerCount = 2;
        startWorker(1);
    }
}

fileCopyDialog::~fileCopyDialog()
{
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }
    if (m_thread2 && m_thread2->isRunning()) {
        m_thread2->quit();
        m_thread2->wait();
    }
    delete ui;
}

void fileCopyDialog::on_cancelButton_clicked()
{
    ui->cancelButton->setEnabled(false);
    if (m_worker) m_worker->cancel();
    if (m_worker2) m_worker2->cancel();
}

void fileCopyDialog::handleProgress(int progress, int done, int cnt, int fail, int del)
{
    QString err;
    if (cnt > 0)
        err += QString(tr("%1 copied. ")).arg(cnt);

    if (fail > 0)
        err += QString(tr("%1 failed. ")).arg(fail);

    if (del > 0)
        err += QString(tr("%1 deleted. ")).arg(del);

    ui->status->setText(
        QString(tr("Copying: %1 of %2 processed. %3")).arg(done).arg(count).arg(err));
    ui->progressBar->setValue(progress);
}

void fileCopyDialog::handleFinished()
{
    handleWorkerFinished();
}


void fileCopyDialog::lastLocationImportedToSlot(QString str)
{
    lastFilePath = str;
}
QString fileCopyDialog::getLastFilePath()
{
    return lastFilePath;
}


void fileCopyDialog::handleProgressFromWorker(int workerIndex, int progress, int done, int cnt, int fail, int del)
{
    if (workerIndex < 0 || workerIndex > 1) return;
    m_done[workerIndex] = done;
    m_cnt[workerIndex]  = cnt;
    m_fail[workerIndex] = fail;
    m_del[workerIndex]  = del;

    int totalDone = m_done[0] + m_done[1];
    int totalCnt  = m_cnt[0]  + m_cnt[1];
    int totalFail = m_fail[0] + m_fail[1];
    int totalDel  = m_del[0]  + m_del[1];

    QString err;
    if (totalCnt > 0)
        err += QString(tr("%1 copied. ")).arg(totalCnt);
    if (totalFail > 0)
        err += QString(tr("%1 failed. ")).arg(totalFail);
    if (totalDel > 0)
        err += QString(tr("%1 deleted. ")).arg(totalDel);

    ui->status->setText(QString(tr("Copying: %1 of %2 processed. %3")).arg(totalDone).arg(m_totalFiles).arg(err));

    // The progress bar is normally driven byte-by-byte via
    // handleBytesAccounted(); fall back to file counts when the total size
    // is unknown (e.g. all files report size 0).
    if (m_totalSourceBytes <= 0) {
        int overallProgress = (m_totalFiles > 0) ? ((totalDone * 100) / m_totalFiles) : progress;
        ui->progressBar->setValue(overallProgress);
    }
}

void fileCopyDialog::handleWorkerFinished()
{
    QObject *s = sender();
    if (s == m_worker && m_thread) {
        m_thread->quit();
    } else if (s == m_worker2 && m_thread2) {
        m_thread2->quit();
    }

    m_finishedWorkers++;
    if ((m_worker && m_worker->wasCancelled()) || (m_worker2 && m_worker2->wasCancelled())) {
        m_cancelRequested = true;
    }
    finalizeIfReady();
}

void fileCopyDialog::handleThreadStopped(int workerIndex)
{
    if (workerIndex == 0) {
        m_thread = nullptr;
        m_worker = nullptr;
    } else if (workerIndex == 1) {
        m_thread2 = nullptr;
        m_worker2 = nullptr;
    }

    m_stoppedThreads++;
    finalizeIfReady();
}

void fileCopyDialog::finalizeIfReady()
{
    if (m_closeScheduled)
        return;

    if (m_finishedWorkers < m_workerCount || m_stoppedThreads < m_workerCount)
        return;

    m_closeScheduled = true;
    if (m_cancelRequested) {
        reject();
    } else {
        accept();
    }
}
