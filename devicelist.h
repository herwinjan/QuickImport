#ifndef DEVICELIST_H
#define DEVICELIST_H

#include <QApplication>
#include <QDialog>
#include <QFileInfoList>
#include <QObject>
#include <QPainter>
#include <QStandardItemModel>
#include <QStringList>
#include <QStringListModel>
#include <qtreeview.h>

#include <filelistmodel.h>

class deviceList : public QTreeView
{
    Q_OBJECT


    FileInfoModel *fileModel;


public:

    deviceList(QWidget *parent = nullptr);
    void addItem(QStandardItem *model, QFileInfo index);

    int countSelected();
    void setFiles(QList<QFileInfo> files);

protected:
    void keyPressEvent(QKeyEvent *event);
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
signals:
    void selectedUpdated(int, qint64);
    void returnButtonPressed();
    void spaceButtonPressed();

    void selectedNode(TreeNode *);
public slots:
    void updateSelectedCount(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles = QList<int>());
};

#endif // DEVICELIST_H
