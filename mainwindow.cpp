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

#include "aboutdialog.h"

#if defined(__APPLE__)
#include "externalDriveFetcher.h"
#endif

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
                                                              << "*.jpeg"
                                                              << "*.mov"
                                                              << "*.mp4"
                                                              << "*.flv"
                                                              << "*.avi"
                                                              << "*.wmv"
                                                              << "*.avchd"
                                                              << "*.heic"
                                                              << "*.srt",
                                                QDir::Files);

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
        ui->inportLocationLabel->setText(tr("<no location set>"));
    else
        ui->inportLocationLabel->setText(importFolder);
    updateImportToLabel();
    setWindowTitle("Quick Import");

    md5Check = settings.value("md5Check", false).toBool();
    ejectAfterImport = settings.value("ejectAfterImport", false).toBool();
    deleteAfterImport = settings.value("deleteAfterImport", false).toBool();
    previewImage = settings.value("previewImage", true).toBool();

    deleteExisting = settings.value("deleteExisting", false).toBool();
    quitEmptyCard = settings.value("quitEmptyCard", false).toBool();
    quitAfterImport = settings.value("quitAfterImport", false).toBool();
    ejectIfEmpty = settings.value("ejectIfEmpty", false).toBool();

    ui->deleteAfterImportBox->setCheckState(deleteAfterImport ? Qt::Checked : Qt::Unchecked);
    ui->ejectBox->setCheckState(ejectAfterImport ? Qt::Checked : Qt::Unchecked);
    ui->mdCheckBox->setCheckState(md5Check ? Qt::Checked : Qt::Unchecked);
    ui->previewImageCheckBox->setCheckState(previewImage ? Qt::Checked : Qt::Unchecked);

    ui->deleteExistingBox->setCheckState(deleteExisting ? Qt::Checked : Qt::Unchecked);
    ui->quitEmptyCardBox->setCheckState(quitEmptyCard ? Qt::Checked : Qt::Unchecked);
    ui->quitAfterImportBox->setCheckState(quitAfterImport ? Qt::Checked : Qt::Unchecked);
    ui->ejectIfEmptyBox->setCheckState(ejectIfEmpty ? Qt::Checked : Qt::Unchecked);

    connect(ui->deviceWidget,
            SIGNAL(selectedUpdated(int, qint64)),
            this,
            SLOT(selectedUpdated(int, qint64)));
    connect(ui->deviceWidget, SIGNAL(spaceButtonPressed()), this, SLOT(spaceButtonPressed()));
    connect(ui->deviceWidget, SIGNAL(returnButtonPressed()), this, SLOT(returnButtonPressed()));

    connect(ui->deviceWidget,
            SIGNAL(selectedNode(TreeNode *)),
            this,
            SLOT(selectedNode(TreeNode *)));

    QKeySequence shortcutKey(Qt::CTRL | Qt::Key_I);

    // Create a shortcut with the specified key sequence
    QShortcut *shortcut = new QShortcut(shortcutKey, this);

    // Connect the activated() signal of the shortcut to your function
    connect(shortcut, &QShortcut::activated, this, &MainWindow::on_moveButton_clicked);

    QKeySequence selectCardKey(Qt::CTRL | Qt::Key_S);
    QShortcut *selectCard = new QShortcut(selectCardKey, this);
    connect(selectCard, &QShortcut::activated, this, &MainWindow::on_selectCard_clicked);

    QKeySequence ejectCardKey(Qt::CTRL | Qt::Key_E);
    QShortcut *ejectCard = new QShortcut(ejectCardKey, this);
    connect(ejectCard, &QShortcut::activated, this, &MainWindow::on_ejectButton_clicked);

    QKeySequence reloadKey(Qt::CTRL | Qt::Key_R);
    QShortcut *reloadCard = new QShortcut(reloadKey, this);
    connect(reloadCard, &QShortcut::activated, this, &MainWindow::on_reloadButton_clicked);

    QMenu *aboutMenu = new QMenu("&About");
    QAction *aboutAction = aboutMenu->addAction("About Quick Import",
                                                this,
                                                &MainWindow::showAboutDialog);

    aboutAction->setMenuRole(QAction::ApplicationSpecificRole);

    setMenuBar(ui->menubar);
    ui->menubar->addMenu(aboutMenu);

    emptyMainWindow();

    show();
    if (!settings.value("dontShowAboutDialog", false).toBool()) {
        showAboutDialog();
    }
    on_selectCard_clicked();
}

void MainWindow::showAboutDialog()
{
    aboutDialog about;
    about.exec();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::selectedUpdated(int cnt, qint64 size)
{
    totalSelectedSize = size;
    ui->spaceFilesCopy->setText(
        QString(tr("%1 GB")).arg(((float) size / 1000 / 1000 / 1000), 0, 'f', 2));
    ui->updateLabel->setText(QString(tr("%1 selected photos")).arg(cnt));

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
void MainWindow::flipSelectedItems()
{
    if (ui->deviceWidget->model()) {
        //QModelIndexList list = ui->deviceWidget->selectionModel()->selectedIndexes();
        QItemSelectionModel *selectionModel = ui->deviceWidget->selectionModel();
        if (selectionModel) {
            QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
            bool selected = true;
            bool set = false;
            for (const QModelIndex &index : selectedIndexes) {
                if (index.isValid()) {
                    // Map the index to get the corresponding QFileInfo
                    QModelIndex sourceIndex = ui->deviceWidget->model()->index(index.row(),
                                                                               0,
                                                                               index.parent());
                    if (sourceIndex.isValid()) {
                        TreeNode *node = static_cast<TreeNode *>(sourceIndex.internalPointer());
                        if (!set) {
                            set = true;
                            selected = !node->isSelected;
                        }
                        node->isSelected = selected;

                        if (!node->isFile) {
                            if (!selected) {
                                static_cast<FileInfoModel *>(ui->deviceWidget->model())
                                    ->setDeselect(node);
                            } else {
                                static_cast<FileInfoModel *>(ui->deviceWidget->model())
                                    ->setSelect(node);
                            }
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
        msgBox.setText(tr("No Card found, please insert card."));
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
    emptyMainWindow();

    if (selectedCard.isValid()) {
        QList<QFileInfo> files;
        QString label;
        statusBar()->showMessage(tr("Loading card..."));
        label = selectedCard.name();
        ui->cardLabel->setText(
            label
            + QString(tr("  (Used space: %1 GB)"))
                  .arg(((float) (selectedCard.bytesTotal() - selectedCard.bytesAvailable()) / 1000
                        / 1000 / 1000),
                       0,
                       'f',
                       2));
        files = getFiles(selectedCard.rootPath());
        ui->deviceWidget->setFiles(files);
        ui->deviceWidget->setEnabled(true);
        statusBar()->showMessage(tr("Done loading card."), 5000);
#if defined(__APPLE__)
        QPixmap pixmap = ExternalDriveIconFetcher::getExternalDrivePixmap(selectedCard.rootPath());

        ui->pixmapLabel->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio));
#endif
        selectedUpdated(0, 0);
        totalSelectedSize = 0;
        ui->moveButton->setDisabled(true);
        ui->ejectButton->setEnabled(true);
        ui->reloadButton->setEnabled(true);
    }
}

QImage requestImage(const QString &id, int height = 0, int width = 0)
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
    if (width == 0 && height == 0) {
        height = screenGeometry.height();
        width = screenGeometry.width();
    }

    return thumbnail.scaled(height / 2, width / 2, Qt::KeepAspectRatio);
}

void MainWindow::displayImage(QString rawFilePath, bool window = true, int h = 0, int w = 0)
{
    statusBar()->showMessage(tr("Loading image, please wait."));
    QImage image = requestImage(rawFilePath, h, w);
    if (window) {
        BorderlessDialog dialog2(image);
        statusBar()->showMessage(tr(""));
        dialog2.exec();
        int lastKey = dialog2.lastKey;

        if (lastKey == Qt::Key_Up || lastKey == Qt::Key_Down || lastKey == Qt::Key_Space) {
            ui->deviceWidget->setFocus();
            QString keyStr(QKeySequence(lastKey).toString()); // key is int with keycode

            QKeyEvent *key_press = new QKeyEvent(QKeyEvent::KeyPress,
                                                 lastKey,
                                                 Qt::NoModifier,
                                                 keyStr);
            QApplication::sendEvent(ui->deviceWidget, key_press);
        }
    } else {
        ui->image->setPixmap(QPixmap::fromImage(image));
    }

    // Create a QDialog and set the label as its central widget
}

void MainWindow::emptyMainWindow()
{
    selectedUpdated(0, 0);
    ui->ejectButton->setDisabled(true);
    ui->reloadButton->setDisabled(true);
    ui->moveButton->setDisabled(true);
    ui->pixmapLabel->setPixmap(QPixmap());
    ui->deviceWidget->setEnabled(false);
    ui->cardLabel->setText(QString(tr("No card loaded.")));
    ui->image->setPixmap(QPixmap());
}

// SLOT for selection of node on ListWidget
void MainWindow::selectedNode(TreeNode *image)
{
    imageSelected = image;
    if (imageSelected && previewImage) {
        qDebug() << "preview";
        if (!imageSelected->isFile) {
            // if not a image, try first child!
            qDebug() << "try first child";
            TreeNode *child = imageSelected->children.at(0);

            qDebug() << child;
            if (child) {
                imageSelected = child;
                image = child;
            }
        }
        if (image->isFile) {
            if (imageLoaderThread) {
                qDebug() << imageLoaderThread->isFinished() << imageLoaderThread->isRunning();
                if (imageLoaderThread->isFinished()) {
                    //imageLoaderThread->quit();
                    imageLoaderThread->wait();
                    delete imageLoaderThread;
                    delete imageLoaderObject;
                } else {
                    return;
                }
            }

            //imageLoader imageLoaderObject;
            imageLoaderObject = new imageLoader();

            //imageLoaderObject.loadImageFile(image->file);

            imageLoaderThread = new QThread(this);

            imageLoaderObject->moveToThread(imageLoaderThread);
            imageShown = image;

            QObject::connect(imageLoaderThread,
                             &QThread::started,
                             imageLoaderObject,
                             &imageLoader::loadImage);

            QObject::connect(imageLoaderObject,
                             &imageLoader::finished,
                             imageLoaderThread,
                             &QThread::quit);

            QObject::connect(imageLoaderObject,
                             &imageLoader::finished,
                             this,
                             &MainWindow::finshedImageLoading);

            /*QObject::connect(imageLoaderObject,
                                   &imageLoader::finished,
                                   imageLoaderThread,
                                   &QThread::deleteLater);
            */

            QObject::connect(imageLoaderObject,
                             &imageLoader::imageLoaded,
                             this,
                             &MainWindow::showImage);
            QObject::connect(imageLoaderObject, &imageLoader::loadingFailed, [=]() {
                ui->image->setPixmap(QPixmap());
                ui->image->setText(tr("Failed to load image."));
            });

            imageLoaderObject->loadImageFile(image->filePath);

            imageLoaderThread->start();

            //   displayImage(image->info.absoluteFilePath(),
            //                false,
            //                ui->groupBoxImport->width() - 10,
            //                ui->groupBoxImport->width() - 10);
        }
    }
}

void MainWindow::finshedImageLoading()
{
    imageLoaderThread->wait();
    qDebug() << "Finished loading.. is latest slected? " << imageSelected << imageShown;
    if (imageSelected != imageShown)
        selectedNode(imageSelected);
}

void MainWindow::showImage(const QImage &image)
{
    ui->image->setPixmap(QPixmap::fromImage(image.scaled(ui->groupBoxImport->width() - 30,
                                                         ui->groupBoxImport->width() - 30,
                                                         Qt::KeepAspectRatio)));
    //label.resize(image.size());
    ui->image->show();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // ui->image->setFixedSize((int) ui->groupBoxImport->width() - 30,
    //                         ui->groupBoxImport->width() - 30);

    // ui->image->show();
    // qDebug() << ui->groupBoxImport->width() - 30;
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
        msgBox.setText(tr("No Import folder set, please set one first."));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
        return;
    }
    if (freeProjectSpace < totalSelectedSize) {
        QMessageBox msgBox;
        msgBox.setText(tr("Not enough diskspace available on project location!"));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
        return;
    }

    if (ui->deviceWidget->model()) {
        QList<QFileInfo> list;
        list = qobject_cast<FileInfoModel *>(ui->deviceWidget->model())->getSelectedFiles();

        if (list.count() <= 0) {
            QMessageBox msgBox;
            msgBox.setText(tr("No files selected, please check files to move/copy."));
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();
            return;
        }

        QDir dir(QString("%1/%2").arg(importFolder, projectName));
        if (!dir.exists())
            dir.mkdir(QString("%1/%2").arg(importFolder, projectName));

        fileCopyDialog dialog(list,
                              importFolder,
                              projectName,
                              md5Check,
                              deleteAfterImport,
                              deleteExisting,
                              this);
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
            qDebug() << "Eject after move";
            on_ejectButton_clicked();
        }

        if (quitAfterImport && ok) {
            qApp->quit();
        }

        reloadCard();
        if (ejectIfEmpty && ok) {
            if (ui->deviceWidget->model()) {
                if (ui->deviceWidget->model()->rowCount(QModelIndex()) <= 0) {
                    doEject();
                }
            } else {
                doEject();
            }
        }

        if (quitEmptyCard && ok) {
            if (ui->deviceWidget->model()) {
                if (ui->deviceWidget->model()->rowCount(QModelIndex()) <= 0) {
                    qApp->quit();
                }
            } else {
                qApp->quit();
            }
        }
    }
}

int MainWindow::doEject()
{
    // Construct the command to eject the USB drive
    QString command = "diskutil";
    QStringList args;
    args << "unmountDisk" << selectedCard.rootPath();

    // Create a QProcess object and start the process
    QProcess process;
    process.start(command, args);
    process.waitForFinished();
    return process.exitCode();
}

void MainWindow::returnButtonPressed()
{
    on_quickViewButton_clicked();
}

void MainWindow::spaceButtonPressed()
{
    flipSelectedItems();
    //on_checkSelected_clicked();
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

                        qDebug() << child;
                        if (child) {
                            if (child->isFile) {
                                displayImage(child->info.absoluteFilePath(), true);
                            }
                        }
                    }
                }
            }
        }
    }
}

void MainWindow::on_ejectButton_clicked()
{
    int code = doEject();

    // Check the exit code of the process
    if (code == 0) {
        // Command executed successfully
        qDebug() << "USB drive ejected successfully.";
        emptyMainWindow();
        ui->deviceWidget->setModel(nullptr);
        selectedCard = QStorageInfo();
        on_selectCard_clicked();
        selectedUpdated(0, 0);

    } else {
        // Error occurred
        qWarning() << "Failed to eject USB drive. Error code:" << code;
    }
}

void MainWindow::on_previewImageCheckBox_stateChanged(int arg1)
{
    previewImage = arg1;
    QSettings settings("HJ Steehouwer", "QuickImport");
    settings.setValue("previewImage", (bool) arg1);
    if (!previewImage)
        ui->image->setPixmap(QPixmap());
}

void MainWindow::on_reloadButton_clicked()
{
    reloadCard();
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
void MainWindow::on_deleteExistingBox_stateChanged(int arg1)
{
    deleteExisting = arg1;
    QSettings settings("HJ Steehouwer", "QuickImport");
    settings.setValue("deleteExisting", (bool) arg1);
}

void MainWindow::on_quitEmptyCardBox_stateChanged(int arg1)
{
    quitEmptyCard = arg1;
    QSettings settings("HJ Steehouwer", "QuickImport");
    settings.setValue("quitEmptyCard", (bool) arg1);
}

void MainWindow::on_ejectIfEmptyBox_stateChanged(int arg1)
{
    ejectIfEmpty = arg1;
    QSettings settings("HJ Steehouwer", "QuickImport");
    settings.setValue("ejectIfEmpty", (bool) arg1);
}

void MainWindow::on_quitAfterImportBox_stateChanged(int arg1)
{
    quitAfterImport = arg1;
    QSettings settings("HJ Steehouwer", "QuickImport");
    settings.setValue("quitAfterImport", (bool) arg1);
}
