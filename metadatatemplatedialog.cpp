#include "metadatatemplatedialog.h"
#include "ui_metadatatemplatedialog.h"

MetaDataTemplateDialog::MetaDataTemplateDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MetaDataTemplateDialog)
{
    ui->setupUi(this);
}

MetaDataTemplateDialog::~MetaDataTemplateDialog()
{
    delete ui;
}
