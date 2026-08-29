#include "shortcutdialog.h"
#include "ui_shortcutdialog.h"

#include <QEvent>

shortcutDialog::shortcutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::shortcutDialog)
{
    ui->setupUi(this);
}

shortcutDialog::~shortcutDialog()
{
    delete ui;
}

void shortcutDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
}
