#include <QApplication>
#include <QAbstractItemModel>
#include <QFileSystemModel>
#include <QTreeView>
#include <QList>
#include <QFileInfo>
#include <QDateTime>
#include <QVariant>
#include <QStyledItemDelegate>
#include "filelistmodel.h"

FileInfoModel::FileInfoModel(const QList<QFileInfo>& fileInfoList, QObject* parent )
        : QAbstractItemModel(parent), m_fileInfoList(fileInfoList) {
        setupModelData();

        view=static_cast<QTreeView*>(parent);
    }

QModelIndex FileInfoModel::index(int row, int column, const QModelIndex& parent ) const  {
        if (!hasIndex(row, column, parent))
            return QModelIndex();


        TreeNode* parentNode;
        if (!parent.isValid())
            parentNode = rootItem;
        else
            parentNode = static_cast<TreeNode*>(parent.internalPointer());

        TreeNode* childNode = parentNode->children.value(row);
        if (childNode)
            return createIndex(row, column, childNode);
        else
            return QModelIndex();
}

QVariant FileInfoModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        // Return header labels for horizontal orientation (columns)
        switch (section) {
        case 0:
            return tr("Files");
        case 1:
            return tr("Time taken");
        case 2:
            return tr("Size");
        // Add more cases for additional columns if needed
        default:
            return QVariant();
        }
    }
    return QVariant();
}

QModelIndex FileInfoModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeNode *childNode = static_cast<TreeNode *>(index.internalPointer());
    TreeNode *parentNode = childNode->parent;

    if (parentNode == rootItem)
        return QModelIndex();

    return createIndex(parentNode->row(), 0, parentNode);
}

int FileInfoModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    TreeNode *parentNode;
    if (!parent.isValid())
        parentNode = rootItem;
    else
        parentNode = static_cast<TreeNode *>(parent.internalPointer());

    return parentNode->children.count();
}

int FileInfoModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 3;
}

QVariant FileInfoModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    TreeNode *node = static_cast<TreeNode *>(index.internalPointer());

    if (role == Qt::DisplayRole) {
        if (index.column() == 0)
            return node->data;
        if (index.column() == 2)
            if (node->isFile)
                return QString("%1 MB").arg(node->info.size() / 1000 / 1000);
        if (index.column() == 1)
            return node->info.birthTime().toString("hh:mm");
    }

    if (role == Qt::CheckStateRole && index.column() == 0) {
        return QVariant(node->isSelected ? Qt::Checked : Qt::Unchecked);
    }

    if (role == Qt::UserRole && index.column() == 0) {
        return node->filePath;
    }

    return QVariant();
}

Qt::ItemFlags FileInfoModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable;
}

bool FileInfoModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::CheckStateRole)
        return false;

    TreeNode *node = static_cast<TreeNode *>(index.internalPointer());
    node->isSelected = (value == Qt::Checked);

    if (!node->isFile) {
        setSelected(node);
        emit dataChanged(QModelIndex(), QModelIndex());
    }

    emit dataChanged(index, index);

    return true;
}

void FileInfoModel::setupModelData()
{
    rootItem = new TreeNode();
    rootItem->data = "Root";

    for (const QFileInfo &fileInfo : m_fileInfoList) {
        QDateTime dateTime = fileInfo.lastModified();
        QString year = QString::number(dateTime.date().year());
        QString month = QString::number(dateTime.date().month());
        QString day = QString::number(dateTime.date().day());
        QString hour = QString::number(dateTime.time().hour()) + QString(" uur");
        QString fileName = fileInfo.fileName();

        TreeNode *yearNode = findOrCreateNode(year, rootItem);
        TreeNode *monthNode = findOrCreateNode(month, yearNode);
        TreeNode *dayNode = findOrCreateNode(day, monthNode);
        TreeNode *hourNode = findOrCreateNode(hour, dayNode);
        TreeNode *fileNode = new TreeNode();
        fileNode->data = fileName;
        fileNode->filePath = fileInfo.filePath();
        fileNode->isSelected = false;
        fileNode->info = fileInfo;
        fileNode->isFile = true;

        fileNode->parent = hourNode;
        hourNode->children.append(fileNode);
    }
}

TreeNode *FileInfoModel::findOrCreateNode(const QString &text, TreeNode *parent)
{
    for (TreeNode *child : parent->children) {
        if (child->data == text)
            return child;
    }

    TreeNode *newNode = new TreeNode();
    newNode->data = text;
    newNode->isSelected = false;
    newNode->parent = parent;
    parent->children.append(newNode);
    return newNode;
}

void FileInfoModel::setSelected(TreeNode *node)
{
    for (TreeNode *child : node->children) {
        child->isSelected = node->isSelected;
        setSelected(child);
    }
}
void FileInfoModel::setSelect(TreeNode *node)
{
    for (TreeNode *child : node->children) {
        child->isSelected = true;
        setSelected(child);
    }
}
void FileInfoModel::setDeselect(TreeNode *node)
{
    for (TreeNode *child : node->children) {
        child->isSelected = false;
        setDeselect(child);
    }
}
void FileInfoModel::selectAll()
{
    setSelect(rootItem);
    emit dataChanged(QModelIndex(), QModelIndex());
}
void FileInfoModel::deSelectAll()
{
    setDeselect(rootItem);
    emit dataChanged(QModelIndex(), QModelIndex());
}
int FileInfoModel::countSelected()
{
    int count = 0;
    count = getCountSelectedItems(this->rootItem, count);
    return count;
}
qint64 FileInfoModel::countSelectedSize()
{
    qint64 size = 0;
    size = getCountSelectedSize(this->rootItem, size);
    qDebug() << "2" << size;
    return size;
}
qint64 FileInfoModel::getCountSelectedSize(TreeNode *node, qint64 size)

{
    qint64 newsize = size;
    for (TreeNode *child : node->children) {
        if (child->isSelected && child->isFile) {
            newsize += child->info.size();
        }
        if (child->children.count() > 0)
            newsize = getCountSelectedSize(child, newsize);
    }
    return newsize;
}
QList<QFileInfo> FileInfoModel::getSelectedFiles()
{
    QList<QFileInfo> list;
    list = getSelectedFilesChilds(this->rootItem, list);
    return list;
}
QList<QFileInfo> FileInfoModel::getSelectedFilesChilds(TreeNode *node, QList<QFileInfo> list)
{
    for (TreeNode *child : node->children) {
        if (child->isSelected && child->isFile)
            list.append(child->info);
        list = getSelectedFilesChilds(child, list);
    }
    return list;
}

int FileInfoModel::getCountSelectedItems(TreeNode *node, int count)
{
    int newcount = count;
    for (TreeNode *child : node->children) {
        if (child->isSelected && child->isFile) {
            ++newcount;
        }
        if (child->children.count() > 0)
            newcount = getCountSelectedItems(child, newcount);
    }
    return newcount;
}
