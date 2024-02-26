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

    this->setModel(fileModel);
    this->expandAll();
    resizeColumnToContents(0);
    // Expand only year and month levels
    QModelIndex rootIndex = fileModel->index(0, 0);
    this->expand(rootIndex);
    for (int i = 0; i < fileModel->rowCount(rootIndex); ++i) {
        QModelIndex yearIndex = fileModel->index(i, 0, rootIndex);
        this->expand(yearIndex);

        for (int j = 0; j < fileModel->rowCount(yearIndex); ++j) {
            QModelIndex monthIndex = fileModel->index(j, 0, yearIndex);
            this->expand(monthIndex);
            for (int j = 0; j < fileModel->rowCount(monthIndex); ++j) {
                QModelIndex dayIndex = fileModel->index(j, 0, monthIndex);
                this->collapse(dayIndex);
            }
        }
    }
    // Assuming treeView is your QTreeView object and columnIndex is the index of the column you want to expand

    // Get the current width of the column
    int currentWidth = columnWidth(0);

    // Add 20px to the current width
    int newWidth = currentWidth + 30;

    // Set the new width for the column
    setColumnWidth(0, newWidth);

    //setSelectionBehavior(QAbstractItemView::SelectItems);
    setSelectionMode(QAbstractItemView::ExtendedSelection); // Allow multiple selections
}

void deviceList::updateSelectedCount(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
    int cnt=fileModel->countSelected();
    emit selectedUpdated(cnt);
}

void deviceList::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return) {
        emit spaceButtonPressed();
        return;
    }
    QTreeView::keyPressEvent(event);
}
