#ifndef FILELISTMODEL_H
#define FILELISTMODEL_H
#include <QApplication>
#include <QAbstractItemModel>
#include <QFileSystemModel>
#include <QTreeView>
#include <QList>
#include <QFileInfo>
#include <QDateTime>
#include <QVariant>
#include <QStyledItemDelegate>

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

    QList<QFileInfo> getSelectedFiles();

private:
    TreeNode* findOrCreateNode(const QString& text, TreeNode* parent) ;

    QList<QFileInfo> m_fileInfoList;
    TreeNode* rootItem;
    QTreeView *view;
    QList<QFileInfo> getSelectedFilesChilds(TreeNode *node, QList<QFileInfo> list);
};

#endif
