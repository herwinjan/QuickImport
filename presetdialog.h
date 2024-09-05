#ifndef PRESETDIALOG_H
#define PRESETDIALOG_H

#include <QDialog>
#include "MainWindow.h"

namespace Ui {
class presetDialog;
}

class presetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit presetDialog(MainWindow *main = nullptr);
    ~presetDialog();

private slots:

    void on_saveButton_clicked();

    void on_removeButton_clicked();

    void on_presetName_textChanged(const QString &arg1);

private:
    Ui::presetDialog *ui;
    MainWindow *main;
    void updatePresetList();
};

#endif // PRESETDIALOG_H
