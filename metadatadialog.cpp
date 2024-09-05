#include "metadatadialog.h"
#include "ui_metadatadialog.h"

MetaDataDialog::MetaDataDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MetaDataDialog)
{
    ui->setupUi(this);
}

MetaDataDialog::~MetaDataDialog()
{
    delete ui;
}
