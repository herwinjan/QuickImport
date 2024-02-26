#ifndef SELECTCARDDIALOG_H
#define SELECTCARDDIALOG_H

#include <QDialog>
#include <QStorageInfo>
#include <qlistwidget.h>

namespace Ui {
class SelectCardDialog;
}

class SelectCardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SelectCardDialog(QWidget *parent = nullptr);
    ~SelectCardDialog();
    void setCards(QList<QStorageInfo>);
    QStorageInfo getSelected();

    QList<QStorageInfo> cardList;

private slots:
    void on_listBox_itemDoubleClicked(QListWidgetItem *item);

private:
    Ui::SelectCardDialog *ui;
};

#endif // SELECTCARDDIALOG_H
