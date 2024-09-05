#include "devicelist.h"
#include <QStringListModel>
#include <QFileInfoList>
#include <QFileInfo>
#include <QDir>
#include <QStandardItem>
#include <QList>
#include <QMouseEvent>
#include <QStyle>
#include <QDir>
#include <QStorageInfo>
#include <QFileSystemModel>
#include <QAbstractItemModel>

deviceList::deviceList(QWidget *parent ) :
    QTreeView(parent)
{}

void deviceList::setFiles(QList<QFileInfo> files)
{
    if (fileModel)
        delete fileModel;
    fileModel=new FileInfoModel(files, this);

    connect(fileModel, &FileInfoModel::dataChanged, this, &deviceList::updateSelectedCount);
    connect(fileModel, &FileInfoModel::treeBuildingFinished, this, &deviceList::expandTree);

    this->setModel(fileModel);
    //
}
void deviceList::expandTree()
{
    this->expandAll();
    // Assuming treeView is your QTreeView object and columnIndex is the index of the column you want to expand
    resizeColumnToContents(0);

    // Expand only year and month levels
    for (int ri = 0; ri < fileModel->rowCount(); ++ri) {
        QModelIndex rootIndex = fileModel->index(ri, 0);
        this->expand(rootIndex);
        for (int i = 0; i < fileModel->rowCount(rootIndex); ++i) {
            QModelIndex yearIndex = fileModel->index(i, 0, rootIndex);
            this->expand(yearIndex);

            for (int j = 0; j < fileModel->rowCount(yearIndex); ++j) {
                QModelIndex monthIndex = fileModel->index(j, 0, yearIndex);
                this->expand(monthIndex);
                for (int t = 0; t < fileModel->rowCount(monthIndex); ++t) {
                    QModelIndex dayIndex = fileModel->index(t, 0, monthIndex);
                    this->collapse(dayIndex);
                }
            }
        }
    }

    // Get the current width of the column
    int currentWidth = columnWidth(0);

    // Add 20px to the current width
    int newWidth = currentWidth;
    //+30;

    // Set the new width for the column
    // setColumnWidth(0, newWidth);

    //setSelectionBehavior(QAbstractItemView::SelectItems);
    setSelectionMode(QAbstractItemView::ExtendedSelection); // Allow multiple selections
    if (fileModel->rowCount() > 0) {
        QModelIndex selectedFirst = fileModel->index(0, 0);
        TreeNode *node = static_cast<TreeNode *>(selectedFirst.internalPointer());
        emit selectedNode(node);
    }
    emit doneLoading();
}

void deviceList::updateSelectedCount(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    Q_UNUSED(roles);
    int cnt = fileModel->countSelected();
    qint64 size = fileModel->countSelectedSize();
    emit selectedUpdated(cnt, size);
}

void deviceList::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return) {
        emit returnButtonPressed();
        return;
    }
    if (event->key() == Qt::Key_Space) {
        emit spaceButtonPressed();
        return;
    }
    QTreeView::keyPressEvent(event);
}

void deviceList::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    if (!selected.isEmpty()) {
        QModelIndex selectedFirst = selected.at(0).topLeft();
        TreeNode *node = static_cast<TreeNode *>(selectedFirst.internalPointer());
        if (node) {
            emit selectedNode(node);
        }
    }

    QTreeView::selectionChanged(selected, deselected);
}
