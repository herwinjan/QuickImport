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
    qDebug() << _cardList;

    ui->listBox->clear();
    if (cardList.isEmpty()) {
        return;
    }

    foreach (const QStorageInfo &storage, cardList) {
        QString text = storage.name()
            + tr(" (size: %1 GB, %2 GB Free )")
                  .arg(storage.bytesTotal() / (1024LL * 1024 * 1024))
                  .arg(storage.bytesFree() / (1024LL * 1024 * 1024));
#if defined(__APPLE__)
        // macOS-specific code
        QListWidgetItem *item = new QListWidgetItem(text);
        item->setIcon(ExternalDriveIconFetcher::getExternalDriveIcon(storage.rootPath()));
#else
        QListWidgetItem *item = new QListWidgetItem(text);
#endif
        ui->listBox->addItem(item);
    }
    if (ui->listBox->count() > 0) {
        ui->listBox->setCurrentRow(0);
    }
}

QStorageInfo SelectCardDialog::getSelected()
{
    int currentItem = ui->listBox->currentRow();
    qDebug() << currentItem;
    if (currentItem < 0 || currentItem >= cardList.size()) {
        return QStorageInfo(); // safe empty value
    }

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
    Q_UNUSED(item);
    accept();
}
