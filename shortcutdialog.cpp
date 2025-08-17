#include "shortcutdialog.h"
#include "ui_shortcutdialog.h"

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
