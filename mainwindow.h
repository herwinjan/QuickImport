#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "filelistmodel.h"
#include "imageloader.h"

#include <QFileInfoList>
#include <QMainWindow>
#include <QShortcut>
#include <QStorageInfo>
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QFileInfoList getFileListFromDir(const QString &directory);

    QList<QFileInfo> getFiles(QString);

    void updateImportToLabel();

    void reloadCard();

    void displayImage(QString rawFilePath, bool, int, int);

    void flipSelectedItems();

protected:
    void resizeEvent(QResizeEvent *event);

    int doEject();

private:
    QString importFolder;
    QString projectName;
    QStorageInfo selectedCard;
    bool md5Check;
    bool deleteAfterImport;
    bool ejectAfterImport;
    bool previewImage;
    qint64 totalSelectedSize = 0;
    qint64 freeProjectSpace = 0;
    Ui::MainWindow *ui;
    void emptyMainWindow();
    imageLoader *imageLoaderObject;
    QThread *imageLoaderThread;
    TreeNode *imageShown, *imageSelected;

    bool deleteExisting;

    bool quitEmptyCard;
    bool ejectIfEmpty;
    bool quitAfterImport;
private slots:
    void showImage(const QImage &image);
    void selectedNode(TreeNode *);
    void showAboutDialog();
    void on_checkSelected_clicked();
    void on_uncheckSelected_clicked();
    void selectedUpdated(int, qint64);
    void on_selectCard_clicked();
    void on_checkAll_clicked();
    void on_uncheckAll_clicked();
    void on_selectLocationButton_clicked();
    void on_projectName_textChanged(const QString &arg1);
    void on_moveButton_clicked();
    void on_mdCheckBox_stateChanged(int arg1);
    void on_deleteAfterImportBox_stateChanged(int arg1);
    void on_ejectBox_stateChanged(int arg1);
    void on_quickViewButton_clicked();
    void spaceButtonPressed();
    void returnButtonPressed();
    void finshedImageLoading();

    void on_ejectButton_clicked();

    void on_previewImageCheckBox_stateChanged(int arg1);
    void on_reloadButton_clicked();
    void on_deleteExistingBox_stateChanged(int arg1);
    void on_quitEmptyCardBox_stateChanged(int arg1);
    void on_ejectIfEmptyBox_stateChanged(int arg1);

    void on_quitAfterImportBox_stateChanged(int arg1);
};
#endif // MAINWINDOW_H
