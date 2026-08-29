#include "fileinfomodel.h"
#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrentMap>
#include <QtEndian>
#include <atomic>
#include <exiv2/exiv2.hpp>
#include <functional>
#include <mutex>
#include <QFileSystemModel>
#include <QList>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QVariant>

namespace {
TreeNode *findOrCreateTreeNode(const QString &text, TreeNode *parent)
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
}

namespace {
// EXIF byte order marker: 0x4949 ("II") = little-endian, 0x4D4D ("MM") = big-endian
bool exifIsBigEndian(unsigned int ord)
{
    return ord == 0x4D4D;
}

quint16 readExifU16(LibRaw_abstract_datastream *data, unsigned int ord)
{
    quint16 v = 0;
    data->read(&v, sizeof(v), 1);
    return exifIsBigEndian(ord) ? qFromBigEndian(v) : qFromLittleEndian(v);
}

quint32 readExifU32(LibRaw_abstract_datastream *data, unsigned int ord)
{
    quint32 v = 0;
    data->read(&v, sizeof(v), 1);
    return exifIsBigEndian(ord) ? qFromBigEndian(v) : qFromLittleEndian(v);
}

double readExifURational(LibRaw_abstract_datastream *data, unsigned int ord)
{
    const quint32 num = readExifU32(data, ord);
    const quint32 den = readExifU32(data, ord);
    return den != 0 ? static_cast<double>(num) / den : 0.0;
}
}

void exif_callback(void *context, int tag, int type, int len, unsigned int ord, void *ifp, long long)
{
    auto *data = static_cast<LibRaw_abstract_datastream *>(ifp);
    auto *mycontext = static_cast<imageInfoStruct *>(context);
    LibRaw_abstract_datastream *stream = (LibRaw_abstract_datastream *) ifp;

    tag &= 0x0fffff; // Undo (ifdN + 1) << 20)
    switch (tag) {
    case 0x8827:                     // ISO Speed Ratings
        if (type == 3 && len == 1) { // 3: unsigned short
            mycontext->isoValue = readExifU16(data, ord);
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
            mycontext->shutterSpeed = readExifURational(data, ord);
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
            mycontext->aperture = readExifURational(data, ord);
        }
        break;
    case 0xA002:                     // Image Width
        if (type == 4 && len == 1) { // 4: unsigned long
            mycontext->resolutionWidth = static_cast<int>(readExifU32(data, ord));
        }
        break;
    case 0xA003:                     // Image Height
        if (type == 4 && len == 1) { // 4: unsigned long
            mycontext->resolutionHeight = static_cast<int>(readExifU32(data, ord));
        }
        break;
    case 0x0103:                     // Compression
        if (type == 3 && len == 1) { // 3: unsigned short
            mycontext->compression = readExifU16(data, ord);
        }
        break;
    case 0xA433: // Lens make
    {
        std::vector<char> buffer(len + 1);
        stream->read(buffer.data(), len, 1);
        buffer[len] = '\0';
        mycontext->lensMake = QString::fromLatin1(buffer.data());
        break;
    }
    case 0xA434: // Lens Model
    {
        std::vector<char> buffer(len + 1);
        stream->read(buffer.data(), len, 1);
        buffer[len] = '\0';
        mycontext->lensModel = QString::fromLatin1(buffer.data());
        break;
    }
    case 0x920A:                     // Focal Length
        if (type == 5 && len == 1) { // 5: unsigned rational
            mycontext->focalLength = readExifURational(data, ord);
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

namespace {
// EXIF fallback for files LibRaw cannot open (JPEG/HEIC/TIFF), so RAW+JPEG
// pairs get the same capture timestamp and never split across folders.
bool parseExifWithExiv2(const QString &path, imageInfoStruct *info)
{
    // Exiv2 is thread-safe for independent images once the XMP parser has
    // been initialized exactly once before concurrent use.
    static std::once_flag xmpInitFlag;
    std::call_once(xmpInitFlag, []() { Exiv2::XmpParser::initialize(); });

    try {
        auto image = Exiv2::ImageFactory::open(
            std::string(QFile::encodeName(path).constData()));
        if (!image)
            return false;
        image->readMetadata();
        const Exiv2::ExifData &exif = image->exifData();
        if (exif.empty())
            return false;

        auto str = [&exif](const char *key) -> QString {
            const auto it = exif.findKey(Exiv2::ExifKey(key));
            return it != exif.end() ? QString::fromStdString(it->toString()).trimmed()
                                    : QString();
        };
        auto rational = [&exif](const char *key, double *out) {
            const auto it = exif.findKey(Exiv2::ExifKey(key));
            if (it == exif.end())
                return;
            const Exiv2::Rational r = it->toRational();
            if (r.second != 0)
                *out = static_cast<double>(r.first) / r.second;
        };

        const QString dt = str("Exif.Photo.DateTimeOriginal");
        if (!dt.isEmpty()) {
            const QDateTime parsed = QDateTime::fromString(dt, "yyyy:MM:dd HH:mm:ss");
            if (parsed.isValid())
                info->dateTimeOriginal = parsed;
        }

        const auto iso = exif.findKey(Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings"));
        if (iso != exif.end())
            info->isoValue = static_cast<int>(iso->toInt64());
        rational("Exif.Photo.ExposureTime", &info->shutterSpeed);
        rational("Exif.Photo.FNumber", &info->aperture);
        rational("Exif.Photo.FocalLength", &info->focalLength);
        if (info->cameraName.isEmpty())
            info->cameraName = str("Exif.Image.Model");
        if (info->serialNumber.isEmpty())
            info->serialNumber = str("Exif.Photo.BodySerialNumber");
        if (info->ownerName.isEmpty())
            info->ownerName = str("Exif.Photo.CameraOwnerName");
        if (info->lensMake.isEmpty())
            info->lensMake = str("Exif.Photo.LensMake");
        if (info->lensModel.isEmpty())
            info->lensModel = str("Exif.Photo.LensModel");

        return info->dateTimeOriginal.isValid();
    } catch (const std::exception &e) {
        qWarning() << "Exiv2 EXIF parse failed for" << path << ":" << e.what();
        return false;
    } catch (...) {
        qWarning() << "Unknown exception in Exiv2 EXIF parse for" << path;
        return false;
    }
}
}

FileInfoModel::~FileInfoModel()
{
    if (m_treeWatcher.isRunning())
        m_treeWatcher.waitForFinished();

    if (m_treeWatcher.future().isFinished()) {
        TreeNode *pendingRoot = m_treeWatcher.result();
        if (pendingRoot && pendingRoot != rootItem)
            delete pendingRoot;
    }

    delete rootItem;
}

FileInfoModel::FileInfoModel(const QList<QFileInfo> &fileInfoList, QObject *parent)
    : QAbstractItemModel(parent)
    , m_fileInfoList(fileInfoList)
    , rootItem(new TreeNode())
{
    qDebug() << "FileInfoModek init:" << m_fileInfoList.count();
    rootItem->data = "Root";
    connect(&m_treeWatcher, &QFutureWatcher<TreeNode *>::finished, this, &FileInfoModel::onTreeBuildingFinished);

    view = static_cast<QTreeView *>(parent);

    // Defer the initial build until the event loop starts so the view has time
    // to connect to our signals before we reset the model.
    QTimer::singleShot(0, this, &FileInfoModel::setupModelData);
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
        case 3:
            return tr("IPTC"); // Add more cases for additional columns if needed
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
    return 4;
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
        if (index.column() == 3)
            if (node->isFile)
                return "✅";
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
        refreshChecks();
    }

    emit dataChanged(index, index);

    return true;
}
QString formatHour(int hour)
{
    QTime time(hour, 0); // Create a QTime object with the given hour and 0 minutes
    QLocale systemLocale = QLocale::system();

    // Check the language and country of the system locale
    if (systemLocale.language() == QLocale::Dutch
        && systemLocale.territory() == QLocale::Netherlands) {
        return systemLocale.toString(time, "h 'uur'");
    } else {
        return systemLocale.toString(time, "hh:mm AP");
    }
}
void FileInfoModel::setupModelData()
{
    const QList<QFileInfo> fileInfoList = m_fileInfoList;
    QPointer<FileInfoModel> self(this);

    auto future = QtConcurrent::run([fileInfoList, self]() -> TreeNode * {
        const int total = fileInfoList.count();
        qDebug() << "Loop Model init:" << total;

        // Phase 1: parse EXIF for all files in parallel on the thread pool.
        // Each file is independent; every functor uses its own LibRaw
        // instance. Status updates are throttled to avoid flooding the GUI
        // event loop with one queued call per file.
        auto counter = std::make_shared<std::atomic_int>(0);
        constexpr int kStatusEvery = 25;

        std::function<imageInfoStruct(const QFileInfo &)> parseExif =
            [self, counter, total](const QFileInfo &fileInfo) -> imageInfoStruct {
            imageInfoStruct info;
            int ret = LIBRAW_SUCCESS;
            try {
                std::unique_ptr<LibRaw> localRawProc(new LibRaw());
                localRawProc->set_exifparser_handler(exif_callback, &info);
                ret = localRawProc->open_file(
                    QFile::encodeName(fileInfo.filePath()).constData());
            } catch (const std::exception &e) {
                ret = LIBRAW_UNSPECIFIED_ERROR;
                qWarning() << "Exception caught during LibRaw processing:" << e.what();
            } catch (...) {
                ret = LIBRAW_UNSPECIFIED_ERROR;
                qWarning() << "Unknown exception caught during LibRaw processing";
            }

            if (ret != LIBRAW_SUCCESS) {
                // Not a RAW file LibRaw understands; JPEG/HEIC/TIFF still
                // carry EXIF — read it with Exiv2 instead.
                static const QSet<QString> exiv2Formats
                    = {"jpg", "jpeg", "heic", "heif", "tif", "tiff"};
                const QString suffix = fileInfo.suffix().toLower();
                if (exiv2Formats.contains(suffix)) {
                    parseExifWithExiv2(fileInfo.filePath(), &info);
                } else {
                    qDebug() << "No EXIF source for" << fileInfo.filePath()
                             << "LibRaw error:" << ret;
                }
            }

            const int done = counter->fetch_add(1, std::memory_order_relaxed) + 1;
            if (self && (done % kStatusEvery == 0 || done == total)) {
                const QString message =
                    self->tr("loading EXIF data #%1 of %2.").arg(done).arg(total);
                QMetaObject::invokeMethod(
                    self,
                    [self, message]() {
                        if (self)
                            emit self->updateProcessStatus(message);
                    },
                    Qt::QueuedConnection);
            }
            return info;
        };

        const QList<imageInfoStruct> exifInfos =
            QtConcurrent::blockingMapped(fileInfoList, parseExif);

        // Phase 2: build the tree sequentially (cheap compared to the EXIF
        // parsing). blockingMapped preserves order, so infos line up with
        // the file list.
        auto *newRoot = new TreeNode();
        newRoot->data = "Root";

        for (int i = 0; i < total; ++i) {
            const QFileInfo &fileInfo = fileInfoList.at(i);
            // Group by capture time (EXIF) when available; fall back to the
            // file's modification time (same rule as the naming tokens).
            const QDateTime exifTime = exifInfos.at(i).dateTimeOriginal;
            const QDateTime dateTime = exifTime.isValid() ? exifTime
                                                          : fileInfo.lastModified();
            const QString year = QString::number(dateTime.date().year());
            const QString month = QString::number(dateTime.date().month());
            const QString day = QString::number(dateTime.date().day());
            const QString hour = formatHour(dateTime.time().hour());

            TreeNode *yearNode = findOrCreateTreeNode(year, newRoot);
            TreeNode *monthNode = findOrCreateTreeNode(month, yearNode);
            TreeNode *dayNode = findOrCreateTreeNode(day, monthNode);
            TreeNode *hourNode = findOrCreateTreeNode(hour, dayNode);
            TreeNode *fileNode = new TreeNode();
            fileNode->data = fileInfo.fileName();
            fileNode->filePath = fileInfo.filePath();
            fileNode->isSelected = false;
            fileNode->info = fileInfo;
            fileNode->isFile = true;
            fileNode->parent = hourNode;
            fileNode->imageInfo = exifInfos.at(i);
            hourNode->children.append(fileNode);
        }

        qDebug() << "Tree structure built." << total;
        return newRoot;
    });

    m_treeWatcher.setFuture(future);
}

void FileInfoModel::collectFileNodes(TreeNode *node, QList<TreeNode *> &fileNodes)
{
    if (node->isFile) {
        fileNodes.append(node);
    }
    for (TreeNode *child : node->children) {
        collectFileNodes(child, fileNodes);
    }
}

void FileInfoModel::refreshChecks()
{
    emit layoutChanged();
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
    refreshChecks();
}
void FileInfoModel::deSelectAll()
{
    setDeselect(rootItem);
    refreshChecks();
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

void FileInfoModel::onTreeBuildingFinished()
{
    TreeNode *newRoot = m_treeWatcher.result();
    if (!newRoot)
        return;

    beginResetModel();
    delete rootItem;
    rootItem = newRoot;
    endResetModel();

    qDebug() << "Tree building finished. Starting EXIF data loading...";
    emit treeBuildingFinished();
}
