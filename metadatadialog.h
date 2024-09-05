#ifndef METADATADIALOG_H
#define METADATADIALOG_H

#include <QDialog>

namespace Ui {
class MetaDataDialog;
}

class MetaDataDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MetaDataDialog(QWidget *parent = nullptr);
    ~MetaDataDialog();

private:
    Ui::MetaDataDialog *ui;
};

#endif // METADATADIALOG_H
