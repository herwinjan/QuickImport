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
    m_thread = new QThread(this);

    ui->setupUi(this);
    ui->status->setText(QString(tr("Copy file %1 of %2.")).arg(0).arg(list.count()));
    ui->progressBar->setRange(0, 100);

    count = list.count();

    ui->progressBar->setValue(0);

    m_worker = new fileCopyWorker(list,
                                  importFolder,
                                  projectName,
                                  fileNameFormat,
                                  md5Check,
                                  deleteAfterImport,
                                  deleteExisting,
                                  importBackupFolder);

    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_worker, &fileCopyWorker::copyImages);
    connect(m_worker, &fileCopyWorker::progressUpdated, this, &fileCopyDialog::handleProgress);
    connect(m_worker,
            &fileCopyWorker::lastLocationImportedTo,
            this,
            &fileCopyDialog::lastLocationImportedToSlot);
    connect(m_worker, &fileCopyWorker::copyingFinished, this, &fileCopyDialog::handleFinished);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_thread->start();
}

fileCopyDialog::~fileCopyDialog()
{
    delete ui;
}

void fileCopyDialog::on_cancelButton_clicked()
{
    m_worker->cancel();
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
    m_thread->quit();
    if (m_worker->doCancel == true) {
        reject();
    }
    accept();
}

void fileCopyDialog::lastLocationImportedToSlot(QString str)
{
    lastFilePath = str;
}
QString fileCopyDialog::getLastFilePath()
{
    return lastFilePath;
}
