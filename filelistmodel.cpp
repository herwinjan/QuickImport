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

void exif_callback(void *context, int tag, int type, int len, unsigned int ord, void *ifp, long long)
{
    auto *data = static_cast<LibRaw_abstract_datastream *>(ifp);
    auto *mycontext = static_cast<imageInfoStruct *>(context);
    LibRaw_abstract_datastream *stream = (LibRaw_abstract_datastream *) ifp;

    tag &= 0x0fffff; // Undo (ifdN + 1) << 20)
    switch (tag) {
    case 0x8827:                     // ISO Speed Ratings
        if (type == 3 && len == 1) { // 3: unsigned short
            unsigned short isoValue;
            data->read(&isoValue, sizeof(isoValue), 1);
            mycontext->isoValue = isoValue;
        }
        break;
    case 0xA430: // Owner Name
    {
        std::vector<char> buffer(len + 1);
        data->read(buffer.data(), len, 1);
        buffer[len] = '\0';
        mycontext->ownerName = QString::fromUtf8(buffer.data());
        break;
    }
    case 0x829A:                     // Exposure Time (shutter speed)
        if (type == 5 && len == 1) { // 5: unsigned rational
            unsigned int num, den;
            data->read(&num, sizeof(num), 1);
            data->read(&den, sizeof(den), 1);
            mycontext->shutterSpeed = static_cast<double>(num) / den;
        }
        break;
    case 0x9201:                      // Shutter speed value (APEX)
        if (type == 10 && len == 1) { // 10: signed rational
            int num, den;
            data->read(&num, sizeof(num), 1);
            data->read(&den, sizeof(den), 1);
            //            mycontext->shutterSpeed = pow(2, static_cast<double>(num) / den);
        }
        break;
    case 0x0110: // Camera Model Name
    {
        std::vector<char> buffer(len + 1);
        data->read(buffer.data(), len, 1);
        buffer[len] = '\0';
        mycontext->cameraName = QString::fromUtf8(buffer.data());
        break;
    }
    case 0x829D:                     // FNumber (aperture)
        if (type == 5 && len == 1) { // 5: unsigned rational
            unsigned int num, den;
            data->read(&num, sizeof(num), 1);
            data->read(&den, sizeof(den), 1);
            mycontext->aperture = static_cast<double>(num) / den;
        }
        break;
    case 0xA002:                     // Image Width
        if (type == 4 && len == 1) { // 4: unsigned long
            unsigned int width;
            data->read(&width, sizeof(width), 1);
            mycontext->resolutionWidth = static_cast<int>(width);
        }
        break;
    case 0xA003:                     // Image Height
        if (type == 4 && len == 1) { // 4: unsigned long
            unsigned int height;
            data->read(&height, sizeof(height), 1);
            mycontext->resolutionHeight = static_cast<int>(height);
        }
        break;
    case 0x0103:                     // Compression
        if (type == 3 && len == 1) { // 3: unsigned short
            unsigned short compressionValue;
            data->read(&compressionValue, sizeof(compressionValue), 1);
            mycontext->compression = compressionValue;
        }
        break;
    case 0xA433: // Lens make
    {
        char lens[128]; // Adjust size as necessary
        stream->read(lens, len, 1);
        lens[len] = '\0';
        mycontext->lensMake = QString::fromLatin1(lens);
        //  qDebug() << mycontext->lensMake;

        break;
    }
    case 0xA434: // Lens Model
    {
        char lens[128]; // Adjust size as necessary
        stream->read(lens, len, 1);
        lens[len] = '\0';
        mycontext->lensModel = QString::fromLatin1(lens);
        //        qDebug() << mycontext->lensModel;

        break;
    }
    case 0x920A:                     // Focal Length
        if (type == 5 && len == 1) { // 5: unsigned rational
            unsigned int num, den;
            data->read(&num, sizeof(num), 1);
            data->read(&den, sizeof(den), 1);
            mycontext->focalLength = static_cast<double>(num) / den;
        }
        break;
    case 0x9003: // DateTimeOriginal
    {
        std::vector<char> buffer(len + 1);
        data->read(buffer.data(), len, 1);
        buffer[len] = '\0';
        QString dateTimeString = QString::fromUtf8(buffer.data());

        // Expected format: "YYYY:MM:DD HH:MM:SS"
        QDateTime dateTime = QDateTime::fromString(dateTimeString, "yyyy:MM:dd HH:mm:ss");
        if (dateTime.isValid()) {
            mycontext->dateTimeOriginal = dateTime;
        } else {
            //   std::cerr << "Invalid date format: " << buffer.data() << std::endl;
        }
    } break;

    case 0xA431: // Camera Serial Number (common)
    case 0xC62F: // Camera Serial Number (for specific brands like Canon)
    {
        std::vector<char> buffer(len + 1);
        data->read(buffer.data(), len, 1);
        buffer[len] = '\0';
        mycontext->serialNumber = QString::fromUtf8(buffer.data());
        break;
    }
    default:
        break;
    }
}

FileInfoModel::FileInfoModel(const QList<QFileInfo> &fileInfoList, QObject *parent)
    : QAbstractItemModel(parent)
    , m_fileInfoList(fileInfoList)
{
    setupModelData();

    view = static_cast<QTreeView *>(parent);
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

        if (!fileNode->rawProc) {
            fileNode->rawProc = new LibRaw();
            fileNode->rawProc->set_exifparser_handler(exif_callback, &fileNode->imageInfo);
            auto state = fileNode->rawProc->open_file(fileNode->filePath.toLatin1().data());
            //    qDebug() << fileNode->imageInfo.ownerName;
            //    qDebug() << fileNode->imageInfo.dateTimeOriginal;
            if (LIBRAW_SUCCESS != state) {
                fileNode->rawProc = NULL;
                delete fileNode->rawProc;
            }
        }
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
QList<fileInfoStruct> FileInfoModel::getSelectedFiles()
{
    QList<fileInfoStruct> list;
    list = getSelectedFilesChilds(this->rootItem, list);
    return list;
}
QList<fileInfoStruct> FileInfoModel::getSelectedFilesChilds(TreeNode *node,
                                                            QList<fileInfoStruct> list)
{
    for (TreeNode *child : node->children) {
        if (child->isSelected && child->isFile) {
            fileInfoStruct item;
            item.fileInfo = child->info;
            item.imageInfo = child->imageInfo;
            list.append(item);
        }
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
