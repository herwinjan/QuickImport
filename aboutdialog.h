#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

namespace Ui {
class aboutDialog;
}

class aboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit aboutDialog(QWidget *parent = nullptr);
    ~aboutDialog();

    bool getdontShowCheckBox();

private slots:
    void on_dontshowCheckBox_stateChanged(int arg1);

private:
    Ui::aboutDialog *ui;
};

#endif // ABOUTDIALOG_H
