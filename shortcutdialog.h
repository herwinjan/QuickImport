#ifndef SHORTCUTDIALOG_H
#define SHORTCUTDIALOG_H

#include <QDialog>

namespace Ui {
class shortcutDialog;
}

class shortcutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit shortcutDialog(QWidget *parent = nullptr);
    ~shortcutDialog();

private:
    Ui::shortcutDialog *ui;
};

#endif // SHORTCUTDIALOG_H
