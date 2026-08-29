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

protected:
    // This dialog is modeless and stays open, so it has to pick up a
    // language change while it is on screen.
    void changeEvent(QEvent *event) override;

private:
    Ui::shortcutDialog *ui;
};

#endif // SHORTCUTDIALOG_H
