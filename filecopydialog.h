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
    fileCopyDialog(const QList<QFileInfo> &list,
                   const QString &importFolder,
                   const QString &projectName,
                   const bool &md5Check,
                   const bool &deleteAfterImport,
                   QWidget *parent = nullptr);
    ~fileCopyDialog();

private slots:
    void on_cancelButton_clicked();

    void handleProgress(int progress, int, int, int, int);
    void handleFinished();

private:
    Ui::fileCopyDialog *ui;
    int count = 0;

    QThread *m_thread;
    fileCopyWorker *m_worker;
};

#endif // FILECOPYDIALOG_H
