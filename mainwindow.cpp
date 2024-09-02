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
#include "fileinfomodel.h"
#include "presetdialog.h"
#include "qborderlessdialog.h"
#include "selectcarddialog.h"
#include "ui_mainwindow.h"

#include <libraw/libraw.h>

#include <QJsonDocument>
#include <QShortcut>

QFileInfoList MainWindow::getFileListFromDir(const QString &directory) {
  QDir qdir(directory);
  QFileInfoList fileList = qdir.entryInfoList(
      QStringList() << "*.3fr" << "*.ari" << "*.arw" << "*.arq" << "*.bay"
                    << "*.braw" << "*.crw" << "*.cr2" << "*.cr3" << "*.cap"
                    << "*.data" << "*.dcs" << "*.dcr" << "*.dng" << "*.drf"
                    << "*.eip" << "*.erf" << "*.fff" << "*.gpr" << "*.heic"
                    << "*.iiq" << "*.k25" << "*.kdc" << "*.mdc" << "*.mef"
                    << "*.mos" << "*.mrw" << "*.nef" << "*.nrw" << "*.obm"
                    << "*.orf" << "*.pef" << "*.ptx" << "*.pxn" << "*.r3d"
                    << "*.raf" << "*.raw" << "*.rwl" << "*.rw2" << "*.rwz"
                    << "*.sr2" << "*.srf" << "*.srw" << "*.tif" << "*.x3f"
                    << "*.jpg" << "*.jpeg" << "*.mov" << "*.mp4" << "*.flv"
                    << "*.avi" << "*.wmv" << "*.wav" << "*.avchd" << "*.heic"
                    << "*.srt",
      QDir::Files);

  for (const QFileInfo &subdir :
       qdir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot)) {

    fileList << getFileListFromDir(
        subdir.absoluteFilePath()); // this is the recursion
  }

  return fileList;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  ui->menubar->hide();
  ui->presets->hide();

  QSettings settings("HJ Steehouwer", "QuickImport");
  loadPresetsLocations();
  loadProjectName();
  loadFileNameFormat();

  //    updateImportToLabel();
  setWindowTitle("Quick Import");

  md5Check = settings.value("md5Check", false).toBool();
  ejectAfterImport = settings.value("ejectAfterImport", false).toBool();
  deleteAfterImport = settings.value("deleteAfterImport", false).toBool();
  previewImage = settings.value("previewImage", true).toBool();

  deleteExisting = settings.value("deleteExisting", false).toBool();
  quitEmptyCard = settings.value("quitEmptyCard", false).toBool();
  quitAfterImport = settings.value("quitAfterImport", false).toBool();
  ejectIfEmpty = settings.value("ejectIfEmpty", false).toBool();
  doBackupImport = settings.value("doBackupImport", false).toBool();

  ui->deleteAfterImportBox->setCheckState(deleteAfterImport ? Qt::Checked
                                                            : Qt::Unchecked);
  ui->ejectBox->setCheckState(ejectAfterImport ? Qt::Checked : Qt::Unchecked);
  ui->mdCheckBox->setCheckState(md5Check ? Qt::Checked : Qt::Unchecked);
  ui->previewImageCheckBox->setCheckState(previewImage ? Qt::Checked
                                                       : Qt::Unchecked);

  ui->deleteExistingBox->setCheckState(deleteExisting ? Qt::Checked
                                                      : Qt::Unchecked);
  ui->quitEmptyCardBox->setCheckState(quitEmptyCard ? Qt::Checked
                                                    : Qt::Unchecked);
  ui->quitAfterImportBox->setCheckState(quitAfterImport ? Qt::Checked
                                                        : Qt::Unchecked);
  ui->ejectIfEmptyBox->setCheckState(ejectIfEmpty ? Qt::Checked
                                                  : Qt::Unchecked);
  ui->backupBox->setCheckState(doBackupImport ? Qt::Checked : Qt::Unchecked);

  connect(ui->deviceWidget, SIGNAL(selectedUpdated(int, qint64)), this,
          SLOT(selectedUpdated(int, qint64)));
  connect(ui->deviceWidget, SIGNAL(spaceButtonPressed()), this,
          SLOT(spaceButtonPressed()));
  connect(ui->deviceWidget, SIGNAL(returnButtonPressed()), this,
          SLOT(returnButtonPressed()));
  connect(ui->deviceWidget, SIGNAL(doneLoading()), this, SLOT(doneLoadingCard()));

  connect(ui->deviceWidget, SIGNAL(selectedNode(TreeNode *)), this, SLOT(selectedNode(TreeNode *)));

  QKeySequence shortcutKey(Qt::CTRL | Qt::Key_I);

  // Create a shortcut with the specified key sequence
  QShortcut *shortcut = new QShortcut(shortcutKey, this);

  // Connect the activated() signal of the shortcut to your function
  connect(shortcut, &QShortcut::activated, this,
          &MainWindow::on_moveButton_clicked);

  QKeySequence selectCardKey(Qt::CTRL | Qt::Key_S);
  QShortcut *selectCard = new QShortcut(selectCardKey, this);
  connect(selectCard, &QShortcut::activated, this,
          &MainWindow::on_selectCard_clicked);

  QKeySequence ejectCardKey(Qt::CTRL | Qt::Key_E);
  QShortcut *ejectCard = new QShortcut(ejectCardKey, this);
  connect(ejectCard, &QShortcut::activated, this,
          &MainWindow::on_ejectButton_clicked);

  QKeySequence reloadKey(Qt::CTRL | Qt::Key_R);
  QShortcut *reloadCard = new QShortcut(reloadKey, this);
  connect(reloadCard, &QShortcut::activated, this,
          &MainWindow::on_reloadButton_clicked);

  QMenu *aboutMenu = new QMenu("&About");
  QAction *aboutAction = aboutMenu->addAction("About Quick Import", this,
                                              &MainWindow::showAboutDialog);

  aboutAction->setMenuRole(QAction::ApplicationSpecificRole);

  setMenuBar(ui->menubar);
  ui->menubar->addMenu(aboutMenu);

  emptyMainWindow();
  loadPresets();

  show();
  if (!settings.value("dontShowAboutDialog", false).toBool()) {
    showAboutDialog();
  }
  on_selectCard_clicked();
}
void MainWindow::updatePresetList() {
  ui->presetComboBox->clear();

  if (ui->presetComboBox->model())
    delete ui->presetComboBox->model();
  presetListModel *model = new presetListModel(presetList);
  ui->presetComboBox->setModel(model);

  ui->presetComboBox->setPlaceholderText(
      QStringLiteral("--Select to load preset--"));
  ui->presetComboBox->setCurrentIndex(-1);
}
void MainWindow::reloadPresetComboBox() {}
void MainWindow::loadPresets() {
  QSettings settings("HJ Steehouwer", "QuickImport");

  // Retrieve the stored QByteArray
  QByteArray storedByteArray = settings.value("presetSettings").toByteArray();

  // Convert the QByteArray back to QJsonArray
  QJsonArray storedJsonArray = QJsonDocument::fromJson(storedByteArray).array();

  // Convert QJsonArray back to QList<presetSetting>
  presetList.clear();

  presetList = jsonArrayToPresetSettings(storedJsonArray);

  updatePresetList();
}
void MainWindow::loadPresetsLocations() {
  QSettings settings("HJ Steehouwer", "QuickImport");
  // settings.setValue("locationPresets", QStringList());
  // settings.setValue("PresetLocationLastUsed", -1);
  importLocationList = settings.value("locationPresets", -1).toStringList();
  importBackupLocationList = settings.value("backupLocationPresets", -1).toStringList();
  int sel = settings.value("PresetLocationLastUsed").toInt();
  resetLocationPreset(sel);
  sel = settings.value("PresetBackupLocationLastUsed").toInt();
  resetBackupLocationPreset(sel);
}
void MainWindow::savePresetsLocations(int sel = -1) {
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("locationPresets", importLocationList);
  settings.setValue("PresetLocationLastUsed", sel);
}
void MainWindow::saveBackupPresetsLocations(int sel = -1)
{
    QSettings settings("HJ Steehouwer", "QuickImport");
    settings.setValue("backupLocationPresets", importBackupLocationList);
    settings.setValue("PresetBackupLocationLastUsed", sel);
}

void MainWindow::savePresets() {
  qDebug() << "save Presets";
  QSettings settings("HJ Steehouwer", "QuickImport");

  // Convert QList<presetSetting> to QJsonArray
  QJsonArray jsonArray = presetSettingsToJsonArray(presetList);

  // Convert QJsonArray to QByteArray
  QByteArray byteArray = QJsonDocument(jsonArray).toJson();
  settings.setValue("presetSettings", byteArray);
}

void MainWindow::slotDeviceAdded(const QString &dev) {
  qDebug("add %s", qPrintable(dev));
  QString devicePath = "/dev/";
  devicePath.append(dev);
  QFileInfo fileInfo(devicePath);

  //  qDebug() << QStorageInfo::mountedVolumes();

  int cnt = 0;
  QString foundStr;
  foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
    qDebug() << storage.device() << devicePath << storage.isValid()
             << storage.isReady();
    if (storage.isValid() &&
        storage.device() == devicePath) { //&& storage.isReady()

      qDebug() << "found";
      cnt++;
    }
  }
  if (cnt == 0) { // not found!
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Card inserted"),
                                  tr("Do you want to open new inserter card?"),
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
      qDebug() << "Yes was clicked";
      on_selectCard_clicked();

    } else {
      qDebug() << "Yes was *not* clicked";
    }
  }
}

void MainWindow::slotDeviceChanged(const QString &dev) {
  qDebug("change %s", qPrintable(dev));
}

void MainWindow::slotDeviceRemoved(const QString &dev) {
  qDebug("remove %s", qPrintable(dev));

  QString devicePath = "/dev/";
  devicePath.append(dev);
  qDebug() << devicePath << selectedCard.device();
  if (selectedCard.device() == devicePath) {
    qDebug() << "reload card";
    selectedCard = QStorageInfo();
    reloadCard();
  }
}

void MainWindow::showAboutDialog() {
  aboutDialog about;
  about.exec();
}

MainWindow::~MainWindow() {
  savePresets();
  delete ui;
}

void MainWindow::selectedUpdated(int cnt, qint64 size) {
  totalSelectedSize = size;
  ui->spaceFilesCopy->setText(
      QString(tr("%1 GB")).arg(((float)size / 1000 / 1000 / 1000), 0, 'f', 2));
  ui->updateLabel->setText(QString(tr("%1 selected photos")).arg(cnt));

  if (cnt <= 0)
    ui->moveButton->setDisabled(true);
  else {
    if (importFolder.count() > 0 && projectName.count() > 0 &&
        fileNameFormat.count() > 0) {
      ui->moveButton->setDisabled(false);
    }

    else {
      ui->moveButton->setDisabled(true);
    }
  }
}

void MainWindow::on_checkSelected_clicked() {
  if (ui->deviceWidget->model()) {
    // QModelIndexList list =
    // ui->deviceWidget->selectionModel()->selectedIndexes();
    QItemSelectionModel *selectionModel = ui->deviceWidget->selectionModel();
    if (selectionModel) {
      QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
      for (const QModelIndex &index : selectedIndexes) {
        if (index.isValid()) {
          // Map the index to get the corresponding QFileInfo
          QModelIndex sourceIndex =
              ui->deviceWidget->model()->index(index.row(), 0, index.parent());
          if (sourceIndex.isValid()) {
            TreeNode *node =
                static_cast<TreeNode *>(sourceIndex.internalPointer());
            node->isSelected = true;

            if (!node->isFile) {
              static_cast<FileInfoModel *>(ui->deviceWidget->model())
                  ->setSelect(node);
            }
            emit ui->deviceWidget->model()->dataChanged(QModelIndex(),
                                                        QModelIndex());
          }
        }
      }
    }
  }
}
void MainWindow::flipSelectedItems() {
  if (ui->deviceWidget->model()) {
    // QModelIndexList list =
    // ui->deviceWidget->selectionModel()->selectedIndexes();
    QItemSelectionModel *selectionModel = ui->deviceWidget->selectionModel();
    if (selectionModel) {
      QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
      bool selected = true;
      bool set = false;
      for (const QModelIndex &index : selectedIndexes) {
        if (index.isValid()) {
          // Map the index to get the corresponding QFileInfo
          QModelIndex sourceIndex =
              ui->deviceWidget->model()->index(index.row(), 0, index.parent());
          if (sourceIndex.isValid()) {
            TreeNode *node =
                static_cast<TreeNode *>(sourceIndex.internalPointer());
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
            emit ui->deviceWidget->model()->dataChanged(QModelIndex(),
                                                        QModelIndex());
          }
        }
      }
    }
  }
}

void MainWindow::on_uncheckSelected_clicked() {
  if (ui->deviceWidget->model()) {
    // QModelIndexList list =
    // ui->deviceWidget->selectionModel()->selectedIndexes();
    QItemSelectionModel *selectionModel = ui->deviceWidget->selectionModel();
    if (selectionModel) {
      QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
      for (const QModelIndex &index : selectedIndexes) {
        if (index.isValid()) {
          // Map the index to get the corresponding QFileInfo
          QModelIndex sourceIndex =
              ui->deviceWidget->model()->index(index.row(), 0, index.parent());
          if (sourceIndex.isValid()) {
            TreeNode *node =
                static_cast<TreeNode *>(sourceIndex.internalPointer());
            node->isSelected = false;
            if (!node->isFile) {
              static_cast<FileInfoModel *>(ui->deviceWidget->model())
                  ->setDeselect(node);
            }
            emit ui->deviceWidget->model()->dataChanged(QModelIndex(),
                                                        QModelIndex());
          }
        }
      }
    }
  }
}

QList<QFileInfo> MainWindow::getFiles(QString map) {
  QList<QFileInfo> files;

  files = getFileListFromDir(map);
  return files;
}

void MainWindow::on_selectCard_clicked() {
  SelectCardDialog window;

  QList<QStorageInfo> cardList;
  ui->deviceWidget->setEnabled(false);

  foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
    if (storage.isReadOnly())
      qDebug() << "isReadOnly:" << storage.isReadOnly();

    qDebug() << "name:" << storage.name();
    qDebug() << "fileSystemType:" << storage.fileSystemType();
    qDebug() << "size:" << storage.bytesTotal() / 1000 / 1000 << "MB";
    qDebug() << "availableSize:" << storage.bytesAvailable() / 1000 / 1000
             << "MB";
    if ((storage.fileSystemType() == "exfat" ||
         storage.fileSystemType() == "fat") &&
        !storage.isReadOnly()) {
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
  if (cardList.count() == 1) {
    selectedCard = cardList.at(0);
  } else {
    window.setCards(cardList);
    if (window.exec()) {
      selectedCard = window.getSelected();
    }
  }
  reloadCard();
}
void MainWindow::doneLoadingCard()
{
    ui->deviceWidget->setEnabled(true);
    statusBar()->showMessage(tr("Done loading card."), 5000);
    selectedUpdated(0, 0);
    totalSelectedSize = 0;
}
void MainWindow::reloadCard() {
  emptyMainWindow();

  if (selectedCard.isValid()) {
    QList<QFileInfo> files;
    QString label;
    statusBar()->showMessage(tr("Loading card..."));
    label = selectedCard.name();
    ui->cardLabel->setText(label +
                           QString(tr("  (Used space: %1 GB)"))
                               .arg(((float)(selectedCard.bytesTotal() -
                                             selectedCard.bytesAvailable()) /
                                     1000 / 1000 / 1000),
                                    0, 'f', 2));
    files = getFiles(selectedCard.rootPath());
    ui->deviceWidget->setFiles(files);
    ui->deviceWidget->setEnabled(false);

#if defined(__APPLE__)
    try {
      QPixmap pixmap = ExternalDriveIconFetcher::getExternalDrivePixmap(
          selectedCard.rootPath());

      ui->pixmapLabel->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio));
    } catch (...) {
    }
#endif

    ui->moveButton->setDisabled(true);
    ui->ejectButton->setEnabled(true);
    ui->reloadButton->setEnabled(true);
  }
}

QImage requestImage(const QString &id, int height = 0, int width = 0) {
  LibRaw rawProc;

  auto state = rawProc.open_file(id.toLatin1().data());
  qDebug() << "State loading Image:" << state;
  QImage thumbnail;
  if (LIBRAW_SUCCESS == state) {
    if (LIBRAW_SUCCESS == rawProc.unpack_thumb()) {
      if (LIBRAW_THUMBNAIL_JPEG == rawProc.imgdata.thumbnail.tformat) {
        thumbnail.loadFromData((unsigned char *)rawProc.imgdata.thumbnail.thumb,
                               rawProc.imgdata.thumbnail.tlength, "JPEG");
      }
    }
    // rawProc.recycle();
  }
  QScreen *screen = QGuiApplication::primaryScreen();
  QRect screenGeometry = screen->geometry();
  if (width == 0 && height == 0) {
    height = screenGeometry.height();
    width = screenGeometry.width();
  }

  return thumbnail.scaled(height / 2, width / 2, Qt::KeepAspectRatio);
}

void MainWindow::displayImage(QString rawFilePath, bool window = true,
                              int h = 0, int w = 0) {
  statusBar()->showMessage(tr("Loading image, please wait."));
  QImage image = requestImage(rawFilePath, h, w);
  if (window) {
    BorderlessDialog dialog2(image);
    statusBar()->showMessage(tr(""));
    dialog2.exec();
    int lastKey = dialog2.lastKey;

    if (lastKey == Qt::Key_Up || lastKey == Qt::Key_Down ||
        lastKey == Qt::Key_Space) {
      ui->deviceWidget->setFocus();
      QString keyStr(
          QKeySequence(lastKey).toString()); // key is int with keycode

      QKeyEvent *key_press =
          new QKeyEvent(QKeyEvent::KeyPress, lastKey, Qt::NoModifier, keyStr);
      QApplication::sendEvent(ui->deviceWidget, key_press);
    }
  } else {
    ui->image->setPixmap(QPixmap::fromImage(image));
  }

  // Create a QDialog and set the label as its central widget
}

void MainWindow::emptyMainWindow() {
  ui->deviceWidget->setModel(nullptr);
  selectedUpdated(0, 0);
  ui->ejectButton->setDisabled(true);
  ui->reloadButton->setDisabled(true);
  ui->moveButton->setDisabled(true);
  ui->pixmapLabel->setPixmap(QPixmap());
  ui->deviceWidget->setEnabled(false);
  ui->cardLabel->setText(QString(tr("No card loaded.")));
  ui->image->setPixmap(QPixmap());
  qDebug() << "doBackupImport" << doBackupImport;
  if (doBackupImport) {
    ui->backupLabel->setEnabled(true);
    ui->importBackupLocation->setEnabled(true);
    ui->deleteBackupLocationButton->setEnabled(true);
    ui->selectBackupLocation->setEnabled(true);

  } else {
    ui->backupLabel->setEnabled(false);
    ui->importBackupLocation->setEnabled(false);
    ui->deleteBackupLocationButton->setEnabled(false);
    ui->selectBackupLocation->setEnabled(false);
  }
}

// SLOT for selection of node on ListWidget
void MainWindow::selectedNode(TreeNode *image) {
  imageSelected = image;
  if (imageSelected && previewImage) {
    qDebug() << "preview";
    if (!imageSelected->isFile) {
      // if not a image, try first child!
      qDebug() << "try first child";
      TreeNode *child = imageSelected->children.at(0);

      qDebug() << child;
      if (child) {
        // imageSelected = child;
        // image = child;
        selectedNode(child);
        return;
      }
    }
    if (image->isFile) {
      if (imageLoaderThread) {
        qDebug() << imageLoaderThread->isFinished()
                 << imageLoaderThread->isRunning();
        if (imageLoaderThread->isFinished()) {
          // imageLoaderThread->quit();
          imageLoaderThread->wait();
          delete imageLoaderThread;
          delete imageLoaderObject;
        } else {
          return;
        }
      }
      currentSelectedImage = image;
      updateImportToLabel();
      // imageLoader imageLoaderObject;
      imageLoaderObject = new imageLoader();

      // imageLoaderObject.loadImageFile(image->file);

      imageLoaderThread = new QThread(this);

      imageLoaderObject->moveToThread(imageLoaderThread);
      imageShown = image;

      QObject::connect(imageLoaderThread, &QThread::started, imageLoaderObject,
                       &imageLoader::loadImage);

      QObject::connect(imageLoaderObject, &imageLoader::finished,
                       imageLoaderThread, &QThread::quit);

      QObject::connect(imageLoaderObject, &imageLoader::finished, this,
                       &MainWindow::finshedImageLoading);

      /*QObject::connect(imageLoaderObject,
                             &imageLoader::finished,
                             imageLoaderThread,
                             &QThread::deleteLater);
      */

      QObject::connect(imageLoaderObject, &imageLoader::imageLoaded, this,
                       &MainWindow::showImage);
      QObject::connect(imageLoaderObject, &imageLoader::loadingFailed, [=]() {
          // ui->image->setPixmap(QPixmap());
          // ui->image->setText(tr("Failed to load image."));
          QImage img = QImage(1024, 1024, QImage::Format_RGB32);
          img.fill(Qt::white);
          img = img.scaled(1024, 1024, Qt::KeepAspectRatio);
          showImage(img);
      });

      imageLoaderObject->loadImageFile(image);

      imageLoaderThread->start();

      //   displayImage(image->info.absoluteFilePath(),
      //                false,
      //                ui->groupBoxImport->width() - 10,
      //                ui->groupBoxImport->width() - 10);
    }
  }
}

void MainWindow::finshedImageLoading() {
  imageLoaderThread->wait();
  qDebug() << "Finished loading.. is latest slected? " << imageSelected
           << imageShown;
  if (imageSelected != imageShown)
    selectedNode(imageSelected);
}

void MainWindow::showImage(const QImage &image, bool failed)
{
    qDebug() << "Show Image";
    // label.resize(image.size());
    QImage img(image);
    QPainter painter(&img);

    // Set font, size, and color
    QFont font("Arial", 30); // You can customize the font and size
    painter.setFont(font);
    QPoint point(50, 50);
    painter.setPen(QColor(Qt::white)); // You can customize the text color

    // Draw text at the specified position
    painter.drawText(10,
                     10,
                     1024,
                     1024,
                     Qt::AlignLeft,
                     QString("%1\nf %2 - 1/%3s\nISO %4\n%5 mm")
                         .arg(currentSelectedImage->info.fileName())
                         .arg(currentSelectedImage->imageInfo.aperture, 0, 'f', 1)
                         .arg(qRound(1.0 / currentSelectedImage->imageInfo.shutterSpeed))
                         .arg(currentSelectedImage->imageInfo.isoValue)
                         .arg(currentSelectedImage->imageInfo.focalLength)

    );

    painter.drawText(10,
                     10,
                     1004,
                     1004,
                     Qt::AlignRight,
                     QString("%1\n%2\n#%3\n%4 %5")
                         .arg(currentSelectedImage->imageInfo.ownerName)
                         .arg(currentSelectedImage->imageInfo.cameraName)
                         .arg(currentSelectedImage->imageInfo.serialNumber)
                         .arg(currentSelectedImage->imageInfo.lensMake)
                         .arg(currentSelectedImage->imageInfo.lensModel)

    );
    if (failed) {
        painter.drawText(0,
                         0,
                         img.width(),
                         img.height(),
                         Qt::AlignCenter,
                         QString(tr("Failed to load image.")));
    }
    ui->image->setPixmap(QPixmap::fromImage(img.scaled(ui->projectGroupBox->width() - 30,
                                                       ui->projectGroupBox->width() - 30,
                                                       Qt::KeepAspectRatio)));
    ui->image->show();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  // ui->image->setFixedSize((int) ui->groupBoxImport->width() - 30,
  //                         ui->groupBoxImport->width() - 30);

  // ui->image->show();
  // qDebug() << ui->groupBoxImport->width() - 30;
}

void MainWindow::on_checkAll_clicked() {
  if (ui->deviceWidget->model())
    qobject_cast<FileInfoModel *>(ui->deviceWidget->model())->selectAll();
}

void MainWindow::on_uncheckAll_clicked() {
  if (ui->deviceWidget->model())
    qobject_cast<FileInfoModel *>(ui->deviceWidget->model())->deSelectAll();
}

void MainWindow::updateImportToLabel() {
  imageInfoStruct imageInfo;
  imageInfo.cameraName = "Test Camera";
  imageInfo.serialNumber = "1233445";
  imageInfo.isoValue = 800;
  QDateTime now = QDateTime::currentDateTime();
  QFileInfo fileinfo;

  if (currentSelectedImage) {
    imageInfo = currentSelectedImage->imageInfo;
    now = currentSelectedImage->info.lastModified();
    fileinfo = currentSelectedImage->info;
  }

  QString example =
      fileCopyWorker::processNewFileName(importFolder, projectName, now,
                                         imageInfo, fileinfo, fileNameFormat)
          .at(2);
  ui->importToLabel->setText(example);
  QDir folder(importFolder);
  QStorageInfo info(folder);

  ui->freeDiskSpace->setText(QString("%1 GB").arg(
      ((float)info.bytesAvailable() / 1000 / 1000 / 1000), 0, 'f', 2));
  freeProjectSpace = info.bytesAvailable();

  if (totalSelectedSize <= 0)
    ui->moveButton->setDisabled(true);
  else {
    if (importFolder.count() > 0 && projectName.count() > 0 &&
        fileNameFormat.count() > 0) {
      ui->moveButton->setDisabled(false);
    }

    else {
      ui->moveButton->setDisabled(true);
    }
  }
}

void MainWindow::on_moveButton_clicked() {
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
    QList<fileInfoStruct> list;
    list = qobject_cast<FileInfoModel *>(ui->deviceWidget->model())
               ->getSelectedFiles();

    if (list.count() <= 0) {
      QMessageBox msgBox;
      msgBox.setText(tr("No files selected, please check files to move/copy."));
      msgBox.setIcon(QMessageBox::Critical);
      msgBox.exec();
      return;
    }
    QString impBackup = importBackupFolder;
    if (!doBackupImport) {
        impBackup = QString();
    }

    fileCopyDialog dialog(list,
                          importFolder,
                          projectName,
                          fileNameFormat,
                          md5Check,
                          deleteAfterImport,
                          deleteExisting,
                          impBackup,
                          this);
    bool ok = dialog.exec();

    /*
    QString delMsg;
    if (didDelete) {
        delMsg = QString(", %1 deleted").arg(del);
    }
    QMessageBox::information(this,
                             "Copying done",
                             QString("copying done, %1 files copied%2, %3
    failed.") .arg(cnt) .arg(delMsg) .arg(fail));*/
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

int MainWindow::doEject() {
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

void MainWindow::returnButtonPressed() { on_quickViewButton_clicked(); }

void MainWindow::spaceButtonPressed() {
  flipSelectedItems();
  // on_checkSelected_clicked();
}

void MainWindow::on_quickViewButton_clicked() {
  if (ui->deviceWidget->model()) {
    // QModelIndexList list =
    // ui->deviceWidget->selectionModel()->selectedIndexes();
    QItemSelectionModel *selectionModel = ui->deviceWidget->selectionModel();
    if (selectionModel) {
      QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
      QModelIndex index = selectedIndexes.at(0);
      if (index.isValid()) {
        // Map the index to get the corresponding QFileInfo
        QModelIndex sourceIndex =
            ui->deviceWidget->model()->index(index.row(), 0, index.parent());
        if (sourceIndex.isValid()) {
          TreeNode *node =
              static_cast<TreeNode *>(sourceIndex.internalPointer());

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

void MainWindow::on_ejectButton_clicked() {
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

void MainWindow::on_previewImageCheckBox_stateChanged(int arg1) {
  previewImage = arg1;
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("previewImage", (bool)arg1);
  if (!previewImage)
    ui->image->setPixmap(QPixmap());
}

void MainWindow::on_reloadButton_clicked() { reloadCard(); }
void MainWindow::on_mdCheckBox_stateChanged(int arg1) {
  md5Check = arg1;
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("md5Check", (bool)arg1);
}

void MainWindow::on_deleteAfterImportBox_stateChanged(int arg1) {
  deleteAfterImport = arg1;
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("deleteAfterImport", (bool)arg1);
}

void MainWindow::on_ejectBox_stateChanged(int arg1) {
  ejectAfterImport = arg1;
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("ejectAfterImport", (bool)arg1);
}
void MainWindow::on_deleteExistingBox_stateChanged(int arg1) {
  deleteExisting = arg1;
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("deleteExisting", (bool)arg1);
}

void MainWindow::on_quitEmptyCardBox_stateChanged(int arg1) {
  quitEmptyCard = arg1;
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("quitEmptyCard", (bool)arg1);
}

void MainWindow::on_ejectIfEmptyBox_stateChanged(int arg1) {
  ejectIfEmpty = arg1;
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("ejectIfEmpty", (bool)arg1);
}

void MainWindow::on_quitAfterImportBox_stateChanged(int arg1) {
  quitAfterImport = arg1;
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("quitAfterImport", (bool)arg1);
}

void MainWindow::on_toolButton_clicked() {
  qDebug() << "Preset clicked";
  presetDialog preset(this);
  preset.exec();
}

void MainWindow::on_presetComboBox_activated(int index) {
  qDebug() << "Activated " << index;
  if (index >= 0) {
    presetSetting nw = presetList.at(ui->presetComboBox->currentIndex());
    if (nw.name.length() > 0) {
      quitAfterImport = nw.quitAfterImport;
      ui->quitAfterImportBox->setCheckState(quitAfterImport ? Qt::Checked
                                                            : Qt::Unchecked);
      md5Check = nw.md5Check;
      ui->mdCheckBox->setCheckState(md5Check ? Qt::Checked : Qt::Unchecked);
      deleteAfterImport = nw.deleteAfterImport;
      ui->deleteAfterImportBox->setCheckState(
          deleteAfterImport ? Qt::Checked : Qt::Unchecked);
      ejectAfterImport = nw.ejectAfterImport;
      ui->ejectBox->setCheckState(ejectAfterImport ? Qt::Checked
                                                   : Qt::Unchecked);
      deleteExisting = nw.deleteExisting;
      ui->deleteExistingBox->setCheckState(deleteExisting ? Qt::Checked
                                                          : Qt::Unchecked);
      quitEmptyCard = nw.quitEmptyCard;
      ui->quitEmptyCardBox->setCheckState(quitEmptyCard ? Qt::Checked
                                                        : Qt::Unchecked);
      ejectIfEmpty = nw.ejectIfEmpty;
      ui->ejectIfEmptyBox->setCheckState(ejectIfEmpty ? Qt::Checked
                                                      : Qt::Unchecked);
      previewImage = nw.previewImage;
      ui->previewImageCheckBox->setCheckState(previewImage ? Qt::Checked
                                                           : Qt::Unchecked);

      updateImportToLabel();
    }
  }
}

void MainWindow::on_projectName_currentTextChanged(const QString &arg1) {
  projectName = arg1;
  updateImportToLabel();
}

void MainWindow::addBackupLocationPreset(QString location)
{
    int sel = -1;

    sel = importBackupLocationList.indexOf(location);
    if (sel == -1) {
        importBackupLocationList.prepend(location);
        sel = 0;
    }
    saveBackupPresetsLocations(sel);
    resetBackupLocationPreset(sel);
}
void MainWindow::addLocationPreset(QString location)
{
    int sel = -1;

    sel = importLocationList.indexOf(location);
    if (sel == -1) {
        importLocationList.prepend(location);
        sel = 0;
    }
    savePresetsLocations(sel);
    resetLocationPreset(sel);
}
void MainWindow::resetBackupLocationPreset(int sel = -1)
{
    ui->importBackupLocation->clear();
    ui->importBackupLocation->addItems(importBackupLocationList);
    if (importBackupLocationList.count() <= 0)
        ui->importBackupLocation->setPlaceholderText(
            QStringLiteral("--Back-up location not set--"));

    if (sel >= 0 && ui->importBackupLocation->count() > 0) {
        ui->importBackupLocation->setCurrentIndex(sel);
        importBackupFolder = importBackupLocationList.at(sel);
    } else {
        sel = 0;
        importBackupFolder = QString();
    }

    updateImportToLabel();
}
void MainWindow::resetLocationPreset(int sel = -1)
{
    ui->importLocation->clear();
    ui->importLocation->addItems(importLocationList);
    if (importLocationList.count() <= 0)
        ui->importLocation->setPlaceholderText(QStringLiteral("--Location not set--"));

    if (sel >= 0 && ui->importLocation->count() > 0) {
        ui->importLocation->setCurrentIndex(sel);
        importFolder = importLocationList.at(sel);
    } else {
        sel = 0;
        importFolder = QString();
    }

    updateImportToLabel();
}
void MainWindow::on_deleteLocationButton_clicked() {
  if (importLocationList.count() > 0)
    importLocationList.remove(ui->importLocation->currentIndex());

  int sel = ui->importLocation->currentIndex();
  if (sel > importLocationList.count() - 1)
    sel = importLocationList.count() - 1;
  if (sel < 0)
    sel = 0;

  savePresetsLocations(sel);
  resetLocationPreset(sel);
}

void MainWindow::on_selectImportLocation_clicked() {
  QString directory = QFileDialog::getExistingDirectory(
      this, tr("Select a directory"), importFolder);
  if (!directory.isEmpty()) {
    importFolder = directory;
    QSettings settings("HJ Steehouwer", "QuickImport");
    settings.setValue("Import Folder", importFolder);
    addLocationPreset(importFolder);

    updateImportToLabel();
  }
}

void MainWindow::on_importLocation_activated(int index) {
  qDebug() << index;
  importFolder = importLocationList.at(index);
  updateImportToLabel();
}

void MainWindow::on_saveProjectNameButton_clicked() {
  int sel = -1;
  sel = projectNameList.indexOf(ui->projectName->currentText());
  if (sel == -1) {
    projectNameList.prepend(ui->projectName->currentText());
    sel = 0;
  }
  saveProjectName(sel);
  resetProjectName(sel);
}
void MainWindow::saveProjectName(int sel = -1) {
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("projectNameList", projectNameList);
  settings.setValue("projectNameLastUsed", sel);
}

void MainWindow::loadProjectName() {
  QSettings settings("HJ Steehouwer", "QuickImport");
  // settings.setValue("locationPresets", QStringList());
  // settings.setValue("PresetLocationLastUsed", -1);
  projectNameList = settings.value("projectNameList").toStringList();
  int sel = settings.value("projectNameLastUsed", -1).toInt();
  if (sel > projectNameList.count() - 1)
    sel = projectNameList.count() - 1;

  resetProjectName(sel);
}
void MainWindow::resetProjectName(int sel = -1) {
  ui->projectName->clear();
  ui->projectName->addItems(projectNameList);
  if (projectNameList.count() <= 0) {
    ui->projectName->setPlaceholderText(
        QStringLiteral("-- set project name --"));
    sel = -1;
  }

  if (sel >= 0) {
    ui->projectName->setCurrentIndex(sel);
    projectName = projectNameList.at(sel);
  } else {
    projectName = QString();
    sel = 0;
  }

  updateImportToLabel();
}

void MainWindow::on_deleteProjectName_clicked() {
  if (projectNameList.count() > 0) {
    projectNameList.remove(ui->projectName->currentIndex());
  }
  int sel = ui->projectName->currentIndex();
  if (sel > projectNameList.count() - 1)
    sel = projectNameList.count() - 1;
  if (sel < 0)
    sel = 0;

  saveProjectName(sel);
  resetProjectName(sel);
}

void MainWindow::on_safeFileNameFormat_clicked() {
  int sel = -1;

  sel = fileNameFormatList.indexOf(ui->fileNameFormat->currentText());
  qDebug() << "Safe" << sel << fileNameFormatList;
  if (sel == -1) {
    fileNameFormatList.prepend(ui->fileNameFormat->currentText());
    sel = 0;
  }
  saveFileNameFormat(sel);
  resetFileNameFomat(sel);
}
void MainWindow::on_deleteFileNameFormat_clicked() {
  if (fileNameFormatList.count() > 0) {
    fileNameFormatList.remove(ui->fileNameFormat->currentIndex());
  }
  int sel = ui->fileNameFormat->currentIndex();
  if (sel > fileNameFormatList.count() - 1)
    sel = fileNameFormatList.count() - 1;
  if (sel < 0)
    sel = 0;
  saveFileNameFormat(sel);
  resetFileNameFomat(sel);
}

void MainWindow::on_fileNameFormat_currentIndexChanged(int index) {
  if (index == -1 || index > fileNameFormatList.count() - 1) {
  } else {
    fileNameFormat = fileNameFormatList.at(index);
  }
  updateImportToLabel();
}

void MainWindow::on_fileNameFormat_currentTextChanged(const QString &arg1) {
  fileNameFormat = arg1;
  updateImportToLabel();
}
void MainWindow::loadFileNameFormat() {
  QSettings settings("HJ Steehouwer", "QuickImport");
  // settings.setValue("fileNameFormat", QStringList());
  // settings.setValue("fileNameFormatLastUsed", -1);
  fileNameFormatList = settings.value("fileNameFormat").toStringList();
  int sel = settings.value("fileNameFormatLastUsed", -1).toInt();

  if (sel > fileNameFormatList.count() - 1)
    sel = fileNameFormatList.count() - 1;
  if (sel < 0 && fileNameFormatList.count() > 0)
    sel = 0;

  if (fileNameFormatList.count() <= 0) {
    sel = 0;
    fileNameFormatList.append("{J}/{o}");
    ui->fileNameFormat->setCurrentIndex(0);
    ui->fileNameFormat->clear();
    ui->fileNameFormat->addItems(fileNameFormatList);
  }

  resetFileNameFomat(sel);
}
void MainWindow::resetFileNameFomat(int sel = -1) {
  if (fileNameFormatList.count() <= 0) {
    sel = -1;
  }

  if (sel >= 0) {
    ui->fileNameFormat->clear();
    ui->fileNameFormat->addItems(fileNameFormatList);
    ui->fileNameFormat->setCurrentIndex(sel);
    fileNameFormat = fileNameFormatList.at(sel);
  } else {
    fileNameFormat = "{J}/{o}";
    fileNameFormatList.append("{J}/{o}");
    ui->fileNameFormat->clear();
    ui->fileNameFormat->addItems(fileNameFormatList);
    ui->fileNameFormat->setCurrentIndex(0);
  }

  updateImportToLabel();
}
void MainWindow::saveFileNameFormat(int sel) {
  qDebug() << "FileNameFormat Save" << fileNameFormatList;
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("fileNameFormat", fileNameFormatList);
  settings.setValue("fileNameFormatLastUsed", sel);
}

void MainWindow::on_shortcutDialogButton_clicked() {
  if (shortcutDialogWindow == nullptr) {
    shortcutDialogWindow = new shortcutDialog();
    shortcutDialogWindow->show();
    connect(shortcutDialogWindow, &shortcutDialog::finished, this,
            &MainWindow::shortcutWindowFinisched);
  } else {
    shortcutDialogWindow->hide();
    delete shortcutDialogWindow;
    shortcutDialogWindow = nullptr;
  }
}
void MainWindow::shortcutWindowFinisched(int) {
  shortcutDialogWindow->hide();
  delete shortcutDialogWindow;
  shortcutDialogWindow = nullptr;
}

void MainWindow::on_backupBox_stateChanged(int arg1) {
  QSettings settings("HJ Steehouwer", "QuickImport");
  settings.setValue("doBackupImport", (bool)arg1);
  doBackupImport = (bool)arg1;
  if (doBackupImport) {
    ui->backupLabel->setEnabled(true);
    ui->importBackupLocation->setEnabled(true);
    ui->deleteBackupLocationButton->setEnabled(true);
    ui->selectBackupLocation->setEnabled(true);

  } else {
      ui->backupLabel->setEnabled(false);
      ui->importBackupLocation->setEnabled(false);
      ui->deleteBackupLocationButton->setEnabled(false);
      ui->selectBackupLocation->setEnabled(false);
  }
}

void MainWindow::on_selectBackupLocation_clicked()
{
    QString directory = QFileDialog::getExistingDirectory(this,
                                                          tr("Select a directory"),
                                                          importFolder);
    if (!directory.isEmpty()) {
        importBackupFolder = directory;
        //QSettings settings("HJ Steehouwer", "QuickImport");
        //settings.setValue("Import Backup Folder", importFolder);
        addBackupLocationPreset(importBackupFolder);

        updateImportToLabel();
    }
}

void MainWindow::on_deleteBackupLocationButton_clicked()
{
    if (importBackupLocationList.count() > 0)
        importBackupLocationList.remove(ui->importBackupLocation->currentIndex());

    int sel = ui->importBackupLocation->currentIndex();
    if (sel > importBackupLocationList.count() - 1)
        sel = importBackupLocationList.count() - 1;
    if (sel < 0)
        sel = 0;

    saveBackupPresetsLocations(sel);
    resetBackupLocationPreset(sel);
}

void MainWindow::on_fileNameFormat_activated(int index)
{
    saveFileNameFormat(index);
}

void MainWindow::on_projectName_activated(int index)
{
    saveProjectName(index);
}
