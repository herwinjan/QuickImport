#include "mainwindow.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QImageReader>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QStorageInfo>

#include "externalDriveFetcher.h"
#include "filecopydialog.h"
#include "filelistmodel.h"
#include "qborderlessdialog.h"
#include "selectcarddialog.h"
#include "ui_mainwindow.h"

#include <libraw/libraw.h>

#include <QShortcut>

QFileInfoList MainWindow::getFileListFromDir(const QString &directory)
{
    QDir qdir(directory);
    QFileInfoList fileList = qdir.entryInfoList(QStringList() << "*.CR2" << "*.CR3", QDir::Files);

    for (const QFileInfo &subdir : qdir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot))
    {

        fileList << getFileListFromDir(subdir.absoluteFilePath()); // this is the recursion
    }

    return fileList;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->menubar->hide();
    QSettings settings("HJ Steehouwer", "QuickImport");
    importFolder = settings.value("Import Folder").toString();
    if (importFolder.length() <= 0)
        ui->inportLocationLabel->setText("<no location set>");
    else
        ui->inportLocationLabel->setText(importFolder);
    updateImportToLabel();
    setWindowTitle("Quick Import");

    md5Check = settings.value("md5Check").toBool();
    ejectAfterImport = settings.value("ejectAfterImport").toBool();
    deleteAfterImport = settings.value("deleteAfterImport").toBool();
    ui->deleteAfterImportBox->setCheckState(deleteAfterImport ? Qt::Checked : Qt::Unchecked);
    ui->ejectBox->setCheckState(ejectAfterImport ? Qt::Checked : Qt::Unchecked);
    ui->mdCheckBox->setCheckState(md5Check ? Qt::Checked : Qt::Unchecked);

    connect(ui->deviceWidget,
            SIGNAL(selectedUpdated(int, qint64)),
            this,
            SLOT(selectedUpdated(int, qint64)));
    connect(ui->deviceWidget, SIGNAL(spaceButtonPressed()), this, SLOT(spaceButtonPressed()));
    this->show();

    QKeySequence shortcutKey(Qt::CTRL | Qt::Key_Return);

    // Create a shortcut with the specified key sequence
    QShortcut *shortcut = new QShortcut(shortcutKey, this);

    // Connect the activated() signal of the shortcut to your function
    connect(shortcut, &QShortcut::activated, this, &MainWindow::on_moveButton_clicked);

    on_selectCard_clicked();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::selectedUpdated(int cnt, qint64 size)
{
    totalSelectedSize = size;
    ui->spaceFilesCopy->setText(
        QString("%1 GB").arg(((float) size / 1000 / 1000 / 1000), 0, 'f', 2));
    ui->updateLabel->setText(QString("%1 selected").arg(cnt));

    if (cnt <= 0)
        ui->moveButton->setDisabled(true);
    else
        ui->moveButton->setDisabled(false);
}

void MainWindow::on_checkSelected_clicked()
{
    if (ui->deviceWidget->model()) {
        //QModelIndexList list = ui->deviceWidget->selectionModel()->selectedIndexes();
        QItemSelectionModel *selectionModel = ui->deviceWidget->selectionModel();
        if (selectionModel) {
            QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
            for (const QModelIndex &index : selectedIndexes) {
                if (index.isValid()) {
                    // Map the index to get the corresponding QFileInfo
                    QModelIndex sourceIndex = ui->deviceWidget->model()->index(index.row(),
                                                                               0,
                                                                               index.parent());
                    if (sourceIndex.isValid()) {
                        TreeNode *node = static_cast<TreeNode *>(sourceIndex.internalPointer());
                        node->isSelected = true;

                        if (!node->isFile) {
                            static_cast<FileInfoModel *>(ui->deviceWidget->model())->setSelect(node);
                        }
                        emit ui->deviceWidget->model()->dataChanged(QModelIndex(), QModelIndex());
                    }
                }
            }
        }
    }
}


void MainWindow::on_uncheckSelected_clicked()
{
    if (ui->deviceWidget->model()) {
        //QModelIndexList list = ui->deviceWidget->selectionModel()->selectedIndexes();
        QItemSelectionModel *selectionModel = ui->deviceWidget->selectionModel();
        if (selectionModel) {
            QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
            for (const QModelIndex &index : selectedIndexes) {
                if (index.isValid()) {
                    // Map the index to get the corresponding QFileInfo
                    QModelIndex sourceIndex = ui->deviceWidget->model()->index(index.row(),
                                                                               0,
                                                                               index.parent());
                    if (sourceIndex.isValid()) {
                        TreeNode *node = static_cast<TreeNode *>(sourceIndex.internalPointer());
                        node->isSelected = false;
                        if (!node->isFile) {
                            static_cast<FileInfoModel *>(ui->deviceWidget->model())
                                ->setDeselect(node);
                        }
                        emit ui->deviceWidget->model()->dataChanged(QModelIndex(), QModelIndex());
                    }
                }
            }
        }
    }
}

QList<QFileInfo> MainWindow::getFiles(QString map)
{
    QList<QFileInfo> files;

    files = getFileListFromDir(map);
    return files;
}

void MainWindow::on_selectCard_clicked()
{
    SelectCardDialog window;

    QList<QStorageInfo> cardList;
    ui->deviceWidget->setEnabled(false);

    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
        if (storage.isReadOnly())
            qDebug() << "isReadOnly:" << storage.isReadOnly();

        qDebug() << "name:" << storage.name();
        qDebug() << "fileSystemType:" << storage.fileSystemType();
        qDebug() << "size:" << storage.bytesTotal() / 1000 / 1000 << "MB";
        qDebug() << "availableSize:" << storage.bytesAvailable() / 1000 / 1000 << "MB";
        if ((storage.fileSystemType() == "exfat" || storage.fileSystemType() == "fat")
            && !storage.isReadOnly()) {
            cardList.append(storage);
        }
    }
    if (cardList.count() < 1) {
        QMessageBox msgBox;
        msgBox.setText("No Card found, please insert card.");
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();

        return;
    }
    window.setCards(cardList);
    if (window.exec()) {
        selectedCard = window.getSelected();
    }
    reloadCard();
}
void MainWindow::reloadCard()
{
    if (selectedCard.isValid()) {
        QList<QFileInfo> files;
        QString label;
        statusBar()->showMessage("Loading card...");
        label = selectedCard.name();
        ui->cardLabel->setText(
            label
            + QString("  (Used space: %1 GB)")
                  .arg(((float) (selectedCard.bytesTotal() - selectedCard.bytesAvailable()) / 1000
                        / 1000 / 1000),
                       0,
                       'f',
                       2));
        files = getFiles(selectedCard.rootPath());
        ui->deviceWidget->setFiles(files);
        ui->deviceWidget->setEnabled(true);
        statusBar()->showMessage("Done loading card.", 5000);
        QPixmap pixmap = ExternalDriveIconFetcher::getExternalDrivePixmap(selectedCard.rootPath());

        ui->pixmapLabel->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio));
        selectedUpdated(0, 0);
        totalSelectedSize = 0;
        ui->moveButton->setDisabled(true);
    }
}

QImage requestImage(const QString &id)
{
    LibRaw rawProc;

    auto state = rawProc.open_file(id.toLatin1().data());
    QImage thumbnail;
    if (LIBRAW_SUCCESS == state) {
        if (LIBRAW_SUCCESS == rawProc.unpack_thumb()) {
            if (LIBRAW_THUMBNAIL_JPEG == rawProc.imgdata.thumbnail.tformat) {
                thumbnail.loadFromData((unsigned char *) rawProc.imgdata.thumbnail.thumb,
                                       rawProc.imgdata.thumbnail.tlength,
                                       "JPEG");
            }
        }
        rawProc.recycle();
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int height = screenGeometry.height();
    int width = screenGeometry.width();

    return thumbnail.scaled(height / 2, width / 2, Qt::KeepAspectRatio);
}

void MainWindow::displayImage(QString rawFilePath)
{
    statusBar()->showMessage(tr("Loading image, please wait."));
    QImage image = requestImage(rawFilePath);
    BorderlessDialog dialog2(image);
    statusBar()->showMessage(tr(""));
    dialog2.exec();
    int lastKey = dialog2.lastKey;

    if (lastKey == Qt::Key_Up || lastKey == Qt::Key_Down || lastKey == Qt::Key_Space) {
        ui->deviceWidget->setFocus();
        QString keyStr(QKeySequence(lastKey).toString()); // key is int with keycode

        QKeyEvent *key_press = new QKeyEvent(QKeyEvent::KeyPress, lastKey, Qt::NoModifier, keyStr);
        QApplication::sendEvent(ui->deviceWidget, key_press);
    }

    // Create a QDialog and set the label as its central widget
}

void MainWindow::on_checkAll_clicked()
{
    if (ui->deviceWidget->model())
        qobject_cast<FileInfoModel *>(ui->deviceWidget->model())->selectAll();
}

void MainWindow::on_uncheckAll_clicked()
{
    if (ui->deviceWidget->model())
        qobject_cast<FileInfoModel *>(ui->deviceWidget->model())->deSelectAll();
}

void MainWindow::on_selectLocationButton_clicked()
{
    QString directory = QFileDialog::getExistingDirectory(this,
                                                          tr("Select a directory"),
                                                          importFolder);
    if (!directory.isEmpty()) {
        qDebug() << "Selected Directory:" << directory;
        importFolder = directory;
        QSettings settings("HJ Steehouwer", "QuickImport");
        settings.setValue("Import Folder", importFolder);
        ui->inportLocationLabel->setText(importFolder);
        updateImportToLabel();
    }
}

void MainWindow::on_projectName_textChanged(const QString &arg1)
{
    projectName = arg1;
    updateImportToLabel();
}
void MainWindow::updateImportToLabel()
{
    ui->importToLabel->setText(QString("%1/%2").arg(importFolder, projectName));
    QDir folder(importFolder);
    QStorageInfo info(folder);

    ui->freeDiskSpace->setText(
        QString("%1 GB").arg(((float) info.bytesAvailable() / 1000 / 1000 / 1000), 0, 'f', 2));
    freeProjectSpace = info.bytesAvailable();
}

void MainWindow::on_moveButton_clicked()
{
    if (importFolder.length() <= 0) {
        QMessageBox msgBox;
        msgBox.setText("No Import folder set, please set one first.");
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
        return;
    }
    if (freeProjectSpace > totalSelectedSize) {
        QMessageBox msgBox;
        msgBox.setText("Not enough diskspace available on project location!");
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
        return;
    }

    if (ui->deviceWidget->model()) {
        QList<QFileInfo> list;
        list = qobject_cast<FileInfoModel *>(ui->deviceWidget->model())->getSelectedFiles();

        if (list.count() <= 0) {
            QMessageBox msgBox;
            msgBox.setText("No files selected, please check files to move/copy.");
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();
            return;
        }

        QDir dir(QString("%1/%2").arg(importFolder, projectName));
        if (!dir.exists())
            dir.mkdir(QString("%1/%2").arg(importFolder, projectName));

        fileCopyDialog dialog(list, importFolder, projectName, md5Check, deleteAfterImport, this);
        bool ok = dialog.exec();

        /*
        QString delMsg;
        if (didDelete) {
            delMsg = QString(", %1 deleted").arg(del);
        }
        QMessageBox::information(this,
                                 "Copying done",
                                 QString("copying done, %1 files copied%2, %3 failed.")
                                     .arg(cnt)
                                     .arg(delMsg)
                                     .arg(fail));*/
        if (ejectAfterImport && ok) {
            // Construct the command to eject the USB drive
            QString command = "diskutil";
            QStringList args;
            args << "unmountDisk" << selectedCard.rootPath();

            // Create a QProcess object and start the process
            QProcess process;
            process.start(command, args);
            process.waitForFinished();

            // Check the exit code of the process
            if (process.exitCode() == 0) {
                // Command executed successfully
                qDebug() << "USB drive ejected successfully.";
                ui->deviceWidget->setModel(nullptr);
                selectedCard = QStorageInfo();
                on_selectCard_clicked();
                selectedUpdated(0, 0);

            } else {
                // Error occurred
                qWarning() << "Failed to eject USB drive. Error code:" << process.exitCode();
            }
        }
        reloadCard();
    }
}

void MainWindow::on_mdCheckBox_stateChanged(int arg1)
{
    md5Check = arg1;
    QSettings settings("HJ Steehouwer", "QuickImport");
    settings.setValue("md5Check", (bool) arg1);
}

void MainWindow::on_deleteAfterImportBox_stateChanged(int arg1)
{
    deleteAfterImport = arg1;
    QSettings settings("HJ Steehouwer", "QuickImport");
    settings.setValue("deleteAfterImport", (bool) arg1);
}

void MainWindow::on_ejectBox_stateChanged(int arg1)
{
    ejectAfterImport = arg1;
    QSettings settings("HJ Steehouwer", "QuickImport");
    settings.setValue("ejectAfterImport", (bool) arg1);
}

void MainWindow::spaceButtonPressed()
{
    on_quickViewButton_clicked();
}

void MainWindow::on_quickViewButton_clicked()
{
    if (ui->deviceWidget->model()) {
        //QModelIndexList list = ui->deviceWidget->selectionModel()->selectedIndexes();
        QItemSelectionModel *selectionModel = ui->deviceWidget->selectionModel();
        if (selectionModel) {
            QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
            QModelIndex index = selectedIndexes.at(0);
            if (index.isValid()) {
                // Map the index to get the corresponding QFileInfo
                QModelIndex sourceIndex = ui->deviceWidget->model()->index(index.row(),
                                                                           0,
                                                                           index.parent());
                if (sourceIndex.isValid()) {
                    TreeNode *node = static_cast<TreeNode *>(sourceIndex.internalPointer());

                    if (node->isFile) {
                        displayImage(node->info.absoluteFilePath());
                    } else {
                        TreeNode *child = node->children.at(0);

                        if (child) {
                            if (child->isFile) {
                                displayImage(child->info.absoluteFilePath());
                            }
                        }
                    }
                }
            }
        }
    }
}
