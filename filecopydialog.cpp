#include "filecopydialog.h"
#include <QCryptographicHash>
#include <QMessageBox>
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
    // Create first thread
    m_thread = new QThread(this);

    ui->setupUi(this);
    ui->progressBar->setRange(0, 100);

    // Total file count across all workers
    count = list.count();
    m_totalFiles = count;

    ui->status->setText(QString(tr("Copy file %1 of %2.")).arg(0).arg(count));
    ui->progressBar->setValue(0);

    // Split the list into up to two halves for parallel copying
    QList<fileInfoStruct> firstHalf;
    QList<fileInfoStruct> secondHalf;
    int half = count / 2;
    if (half > 0) {
        firstHalf = list.mid(0, half);
        secondHalf = list.mid(half);
    } else {
        firstHalf = list;
    }

    m_workerCount = secondHalf.isEmpty() ? 1 : 2;

    // Worker 1
    m_worker = new fileCopyWorker(firstHalf,
                                  importFolder,
                                  projectName,
                                  fileNameFormat,
                                  md5Check,
                                  deleteAfterImport,
                                  deleteExisting,
                                  importBackupFolder);
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_worker, &fileCopyWorker::copyImages);
    connect(m_worker, &fileCopyWorker::progressUpdated, this,
            [this](int progress, int done, int cnt, int fail, int del){
                handleProgressFromWorker(0, progress, done, cnt, fail, del);
            });
    connect(m_worker, &fileCopyWorker::lastLocationImportedTo,
            this, &fileCopyDialog::lastLocationImportedToSlot);
    connect(m_worker, &fileCopyWorker::errorOccurred, this, [this](const QString &msg){
               QMessageBox::critical(this, tr("Error"), msg);
           }, Qt::QueuedConnection);
    connect(m_worker, &fileCopyWorker::copyingFinished, this, &fileCopyDialog::handleWorkerFinished);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    m_thread->start();

    // Worker 2 (optional)
    if (!secondHalf.isEmpty()) {
        m_thread2 = new QThread(this);
        m_worker2 = new fileCopyWorker(secondHalf,
                                       importFolder,
                                       projectName,
                                       fileNameFormat,
                                       md5Check,
                                       deleteAfterImport,
                                       deleteExisting,
                                       importBackupFolder);
        m_worker2->moveToThread(m_thread2);
        connect(m_thread2, &QThread::started, m_worker2, &fileCopyWorker::copyImages);
        connect(m_worker2, &fileCopyWorker::progressUpdated, this,
                [this](int progress, int done, int cnt, int fail, int del){
                    handleProgressFromWorker(1, progress, done, cnt, fail, del);
                });
        connect(m_worker2, &fileCopyWorker::lastLocationImportedTo,
                this, &fileCopyDialog::lastLocationImportedToSlot);
        connect(m_worker2, &fileCopyWorker::errorOccurred, this, [this](const QString &msg){
                   QMessageBox::critical(this, tr("Error"), msg);
               }, Qt::QueuedConnection);
        connect(m_worker2, &fileCopyWorker::copyingFinished, this, &fileCopyDialog::handleWorkerFinished);
        connect(m_thread2, &QThread::finished, m_worker2, &QObject::deleteLater);
        connect(m_thread2, &QThread::finished, m_thread2, &QObject::deleteLater);
        m_thread2->start();
    }

}

fileCopyDialog::~fileCopyDialog()
{
    delete ui;
}

void fileCopyDialog::on_cancelButton_clicked()
{
    m_worker->cancel();
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

    int overallProgress = (m_totalFiles > 0) ? ((totalDone * 100) / m_totalFiles) : progress;

    QString err;
    if (totalCnt > 0)
        err += QString(tr("%1 copied. ")).arg(totalCnt);
    if (totalFail > 0)
        err += QString(tr("%1 failed. ")).arg(totalFail);
    if (totalDel > 0)
        err += QString(tr("%1 deleted. ")).arg(totalDel);

    ui->status->setText(QString(tr("Copying: %1 of %2 processed. %3")).arg(totalDone).arg(m_totalFiles).arg(err));
    ui->progressBar->setValue(overallProgress);
}

void fileCopyDialog::handleWorkerFinished()
{
    // Quit the thread of the sender
    QObject *s = sender();
    if (s == m_worker && m_thread) {
        m_thread->quit();
    } else if (s == m_worker2 && m_thread2) {
        m_thread2->quit();
    }

    m_finishedWorkers++;
    if ((m_worker && m_worker->doCancel) || (m_worker2 && m_worker2->doCancel)) {
        reject();
        return;
    }
    if (m_finishedWorkers >= m_workerCount) {
        accept();
    }
}
