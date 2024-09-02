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
};

#endif // FILECOPYDIALOG_H
