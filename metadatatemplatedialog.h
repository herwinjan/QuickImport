#ifndef METADATATEMPLATEDIALOG_H
#define METADATATEMPLATEDIALOG_H

#include <QDialog>

namespace Ui {
class MetaDataTemplateDialog;
}

class MetaDataTemplateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MetaDataTemplateDialog(QWidget *parent = nullptr);
    ~MetaDataTemplateDialog();

private:
    Ui::MetaDataTemplateDialog *ui;
};

#endif // METADATATEMPLATEDIALOG_H
