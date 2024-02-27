#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFileInfoList>
#include <QMainWindow>
#include <QStorageInfo>

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

    void displayImage(QString rawFilePath);

private:
    QString importFolder;
    QString projectName;
    QStorageInfo selectedCard;
    bool md5Check;
    bool deleteAfterImport;
    bool ejectAfterImport;
    qint64 totalSelectedSize = 0;
    qint64 freeProjectSpace = 0;
    Ui::MainWindow *ui;
public slots:

private slots:
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
};
#endif // MAINWINDOW_H
