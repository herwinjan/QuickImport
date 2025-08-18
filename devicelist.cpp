#include "devicelist.h"
#include <QStringListModel>
#include <QFileInfoList>
#include <QFileInfo>
#include <QDir>
#include <QStandardItem>
#include <QList>
#include <QMouseEvent>
#include <QStyle>
#include <QStorageInfo>
#include <QFileSystemModel>
#include <QAbstractItemModel>
#include <QDebug>
#include <QKeyEvent>
#include <QItemSelection>
#include <QHeaderView>

deviceList::deviceList(QWidget *parent ) :
    QTreeView(parent)
{
    // Ensure pointer is initialized to avoid undefined behavior on first use
    fileModel = nullptr;

    // Configure once rather than on every expand
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Make columns auto-size to fit their content
    if (auto *hdr = header()) {
        hdr->setSectionResizeMode(QHeaderView::ResizeToContents);
        hdr->setStretchLastSection(false);
    }

    // Optional performance hint for tree views with consistent row heights
    setUniformRowHeights(true);
}

void deviceList::setFiles(QList<QFileInfo> files)
{
    qDebug() << "SetFiles" << files.count();
    if (fileModel) {
        // detach model from the view before deleting to avoid use-after-free
        this->setModel(nullptr);
        delete fileModel;
        fileModel = nullptr;
    }
    fileModel = new FileInfoModel(files, this);

    connect(fileModel, &FileInfoModel::dataChanged, this, &deviceList::updateSelectedCount);
    connect(fileModel, &FileInfoModel::treeBuildingFinished, this, &deviceList::expandTree);

    this->setModel(fileModel);
    // Ensure columns fit contents after (re)setting the model
    const int cols = fileModel->columnCount();
    for (int c = 0; c < cols; ++c) {
        resizeColumnToContents(c);
    }
}
void deviceList::expandTree()
{
    if (!fileModel)
        return;

    // Expand only the relevant levels (year and month) and size the first column
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
    // int currentWidth = columnWidth(0);

    // Add 20px to the current width
    // int newWidth = currentWidth;
    //+30;

    // Set the new width for the column
    // setColumnWidth(0, newWidth);

    if (fileModel->rowCount() > 0) {
        const QModelIndex selectedFirst = fileModel->index(0, 0);
        if (selectedFirst.isValid()) {
            if (auto *node = static_cast<TreeNode *>(selectedFirst.internalPointer())) {
                emit selectedNode(node);
            }
        }
    }
    // After expanding, ensure all columns are sized to fit current contents
    if (fileModel) {
        const int cols = fileModel->columnCount();
        for (int c = 0; c < cols; ++c) {
            resizeColumnToContents(c);
        }
    }
    emit doneLoading();
}

void deviceList::updateSelectedCount(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    Q_UNUSED(roles);
    if (!fileModel) return;
    int cnt = fileModel->countSelected();
    qint64 size = fileModel->countSelectedSize();
    emit selectedUpdated(cnt, size);
}

void deviceList::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
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
        const QModelIndex selectedFirst = selected.at(0).topLeft();
        if (selectedFirst.isValid()) {
            if (auto *node = static_cast<TreeNode *>(selectedFirst.internalPointer())) {
                emit selectedNode(node);
            }
        }
    }

    QTreeView::selectionChanged(selected, deselected);
}
