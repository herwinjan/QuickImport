#include "filesmodel.h"
#include <QDir>
#include <QStorageInfo>
#include <QFileSystemModel>


filesModel::filesModel(QObject *parent)
    : QAbstractItemModel(parent)
{

    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
        qDebug() << storage.rootPath();
        if (storage.isReadOnly())
            qDebug() << "isReadOnly:" << storage.isReadOnly();

        qDebug() << "name:" << storage.name();
        qDebug() << "fileSystemType:" << storage.fileSystemType();
        qDebug() << "size:" << storage.bytesTotal()/1000/1000 << "MB";
        qDebug() << "availableSize:" << storage.bytesAvailable()/1000/1000 << "MB";
        if (storage.fileSystemType()=="exfat")
        {
            qDebug()<<"get List"<<storage.rootPath();

            files=getFileListFromDir(storage.rootPath());
            qDebug()<<files.count();
        }

    }

}

int filesModel::rowCount(const QModelIndex& parent ) const
{
    return files.count();
}
int filesModel::columnCount(const QModelIndex& parent) const
{
    return 2;

}
QVariant filesModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::DisplayRole) {
        if (index.column() == 0) {
            return files.at(index.row()).baseName();
        } else if (index.column() == 1) {
            return QString("2");
        }

    } else if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;
    return QVariant();

}
QVariant filesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Vertical) {
            if (section >= 0)
                return QString::number(section);
        } else {

            switch (section) {
            case 0:
                return QString(tr("Selection"));
                break;
            case 1:
                return QString(tr("Date"));
                break;
            }
        }
    }
    return QVariant();

}

void filesModel::doubleClickedSlot(const QModelIndex&)
{

}
QModelIndex filesModel::parent(const QModelIndex &child) const
{
    //return innerModel->parent(child);
    return QModelIndex();
}

QModelIndex filesModel::index(int row, int column, const QModelIndex &parent) const
{
    return QModelIndex();
    //return createIndex(row, column, parent);


    //qDebug()<<row<<column;
//    if (parent.isValid()) // It's a list. Not a tree
      //     return QModelIndex();
   //    return createIndex(row, column); // Create a index for your own model.
}

// ----------

QFileInfoList filesModel::getFileListFromDir(const QString &directory)
{
    QDir qdir(directory);
    QFileInfoList fileList = qdir.entryInfoList(QStringList() << "*.3fr"
                                                              << "*.ari"
                                                              << "*.arw"
                                                              << "*.arq"
                                                              << "*.bay"
                                                              << "*.braw"
                                                              << "*.crw"
                                                              << "*.cr2"
                                                              << "*.cr3"
                                                              << "*.cap"
                                                              << "*.data"
                                                              << "*.dcs"
                                                              << "*.dcr"
                                                              << "*.dng"
                                                              << "*.drf"
                                                              << "*.eip"
                                                              << "*.erf"
                                                              << "*.fff"
                                                              << "*.gpr"
                                                              << "*.iiq"
                                                              << "*.k25"
                                                              << "*.kdc"
                                                              << "*.mdc"
                                                              << "*.mef"
                                                              << "*.mos"
                                                              << "*.mrw"
                                                              << "*.nef"
                                                              << "*.nrw"
                                                              << "*.obm"
                                                              << "*.orf"
                                                              << "*.pef"
                                                              << "*.ptx"
                                                              << "*.pxn"
                                                              << "*.r3d"
                                                              << "*.raf"
                                                              << "*.raw"
                                                              << "*.rwl"
                                                              << "*.rw2"
                                                              << "*.rwz"
                                                              << "*.sr2"
                                                              << "*.srf"
                                                              << "*.srw"
                                                              << "*.tif"
                                                              << "*.x3f"
                                                              << "*.jpg"
                                                              << "*.jpeg",
                                                QDir::Files);

    for (const QFileInfo &subdir : qdir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot))
    {
        files << getFileListFromDir(subdir.absoluteFilePath()); // this is the recursion
    }

    return fileList;
}

