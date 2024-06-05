#ifndef FILELISTMODEL_H
#define FILELISTMODEL_H
#include <QAbstractItemModel>
#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QList>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QVariant>
#include <libraw/libraw.h>

struct imageInfoStruct
{
    int isoValue = 0;
    QString ownerName;
    double shutterSpeed = 0.0;
    QString cameraName;
    double aperture = 0.0;
    int resolutionWidth = 0;
    int resolutionHeight = 0;
    int compression = 0;
    QString lensMake;
    QString lensModel;

    double focalLength = 0.0;
    QDateTime dateTimeOriginal;
    QString serialNumber;
};

struct TreeNode {
    QString data;
    QString filePath;
    TreeNode* parent;
    QFileInfo info;
    bool isFile=false;

    bool isSelected;
    QList<TreeNode*> children;
    int row() const {
        if (parent)
            return parent->children.indexOf(const_cast<TreeNode*>(this));
        return 0;
    }

    imageInfoStruct imageInfo;

    LibRaw *rawProc = NULL;
};

struct fileInfoStruct
{
    QFileInfo fileInfo;
    imageInfoStruct imageInfo;
};

class FileInfoModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit FileInfoModel(const QList<QFileInfo>& fileInfoList, QObject* parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override ;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override ;

    QModelIndex parent(const QModelIndex& index) const override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override ;

    int columnCount(const QModelIndex& parent = QModelIndex()) const override ;
    QVariant data(const QModelIndex& index, int role) const override;

    Qt::ItemFlags flags(const QModelIndex& index) const override ;

    bool setData(const QModelIndex& index, const QVariant& value, int role) override ;

    void setupModelData() ;

    void setSelected(TreeNode* node);
    int countSelected();
    int getCountSelectedItems(TreeNode *node, int count);

    void setSelect(TreeNode *node);
    void setDeselect(TreeNode *node);

    void selectAll();

    void deSelectAll();

    QList<fileInfoStruct> getSelectedFiles();

    qint64 countSelectedSize();

    qint64 getCountSelectedSize(TreeNode *node, qint64 size);

private:
    TreeNode* findOrCreateNode(const QString& text, TreeNode* parent) ;

    QList<QFileInfo> m_fileInfoList;
    TreeNode* rootItem;
    QTreeView *view;
    QList<fileInfoStruct> getSelectedFilesChilds(TreeNode *node, QList<fileInfoStruct> list);
};

#endif
