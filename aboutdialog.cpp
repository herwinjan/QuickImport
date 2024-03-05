#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include <QSettings>

aboutDialog::aboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::aboutDialog)
{
    ui->setupUi(this);
    QSettings settings("HJ Steehouwer", "QuickImport");
    ui->dontshowCheckBox->setCheckState(
        settings.value("dontShowAboutDialog", false).toBool() ? Qt::Checked : Qt::Unchecked);
    ui->aboutBox->setTitle(tr("About Quick Import (version %1)").arg("0.5"));
}

aboutDialog::~aboutDialog()
{
    delete ui;
}

bool aboutDialog::getdontShowCheckBox()
{
    return ui->dontshowCheckBox->isChecked();
}

void aboutDialog::on_dontshowCheckBox_stateChanged(int arg1)
{
    QSettings settings("HJ Steehouwer", "QuickImport");
    settings.setValue("dontShowAboutDialog", (bool) arg1);
}
