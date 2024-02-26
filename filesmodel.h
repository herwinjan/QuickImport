#ifndef FILESMODEL_H
#define FILESMODEL_H

#include <QAbstractItemModel>
#include <QFileInfo>
#include <QList>
#include <QModelIndex>

class filesModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit filesModel(QObject *parent = nullptr);

    QFileInfoList files;


    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

private:

    QFileInfoList getFileListFromDir(const QString &directory);

public slots:
    void doubleClickedSlot(const QModelIndex&);

signals:
    void updateStatus();

};

#endif // FILESMODEL_H
