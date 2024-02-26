#include "selectcarddialog.h"
#include "externalDriveFetcher.h"
#include "ui_selectcarddialog.h"

SelectCardDialog::SelectCardDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SelectCardDialog)
{
    ui->setupUi(this);
    ui->listBox->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->listBox->setSelectionBehavior(QAbstractItemView::SelectItems);
}

void SelectCardDialog::setCards(QList<QStorageInfo> _cardList)
{
    cardList = _cardList;
    foreach (const QStorageInfo &storage, cardList) {
        QListWidgetItem *item = new QListWidgetItem(
            storage.name()
            + QString(" (size: %1 GB, %2 GB Free )")
                  .arg(storage.bytesTotal() / 1000 / 1000 / 1000)
                  .arg(storage.bytesFree() / 1000 / 1000 / 1000));
        item->setIcon(ExternalDriveIconFetcher::getExternalDriveIcon(storage.rootPath()));
        ui->listBox->addItem(item);
    }
    ui->listBox->setCurrentRow(0);
}

QStorageInfo SelectCardDialog::getSelected()
{
    int currentItem = ui->listBox->currentRow();
    qDebug() << currentItem;
    return cardList.at(currentItem);

    //QListWidgetItem *current = ui->listBox->currentItem();
    //qDebug() << current->text();

    //foreach (const QStorageInfo &storage, cardList) {
    //    if (current->text() == storage.rootPath()) {
    //        return storage;
    //    }
    //}
    //return QStorageInfo();
}

SelectCardDialog::~SelectCardDialog()
{
    delete ui;
}

void SelectCardDialog::on_listBox_itemDoubleClicked(QListWidgetItem *item)
{
    accept();
}
