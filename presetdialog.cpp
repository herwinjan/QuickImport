#include "presetdialog.h"
#include "ui_presetdialog.h"

presetDialog::presetDialog(MainWindow *main)
    : QDialog(main)
    , ui(new Ui::presetDialog)
    , main(main)
{
    ui->setupUi(this);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updatePresetList();
    ui->saveButton->setDisabled(true);
}

void presetDialog::updatePresetList()
{
    ui->presetList->clear();
    for (const presetSetting item : main->presetList) {
        ui->presetList->addItem(item.name);
    }
    if (main->presetList.length() > 0) {
        ui->removeButton->setEnabled(true);
    } else {
        ui->removeButton->setDisabled(true);
    }
}

presetDialog::~presetDialog()
{
    delete ui;
}

void presetDialog::on_saveButton_clicked()
{
    if (ui->presetName->text().length() > 0) {
        qDebug() << "add preset" << ui->presetName->text();
        presetSetting nw;
        nw.name = ui->presetName->text();

        nw.deleteAfterImport = main->deleteAfterImport;
        nw.ejectAfterImport = main->ejectAfterImport;
        nw.importFolder = main->importFolder;
        nw.quitAfterImport = main->quitAfterImport;
        nw.deleteExisting = main->deleteExisting;
        nw.importFolder = main->importFolder;
        nw.ejectIfEmpty = main->ejectIfEmpty;
        nw.md5Check = main->md5Check;
        nw.previewImage = main->previewImage;
        nw.quitEmptyCard = main->quitEmptyCard;

        main->presetList.append(nw);
        main->updatePresetList();
        accept();
    }
}

void presetDialog::on_removeButton_clicked()
{
    int item = ui->presetList->currentRow();
    qDebug() << "remove " << item;
    if (item >= 0) {
        main->presetList.removeAt(item);
        main->updatePresetList();
        updatePresetList();
    }
}

void presetDialog::on_presetName_textChanged(const QString &arg1)
{
    if (arg1.length() > 0) {
        ui->saveButton->setEnabled(true);
    } else {
        ui->saveButton->setDisabled(true);
    }
}
