#include "mainwindow.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
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
#include <QTimer>

QFileInfoList MainWindow::getFileListFromDir(const QString &directory) {
    QDir qdir(directory);
    // Files only, readable, do not follow symlinks (prevents odd loops)
    qdir.setFilter(QDir::Files | QDir::NoSymLinks | QDir::Readable);
    const QStringList patterns = QStringList()
        << "*.3fr" << "*.ari" << "*.arw" << "*.arq" << "*.bay"
        << "*.braw" << "*.crw" << "*.cr2" << "*.cr3" << "*.cap"
        << "*.data" << "*.dcs" << "*.dcr" << "*.dng" << "*.drf"
        << "*.eip" << "*.erf" << "*.fff" << "*.gpr" << "*.heic"
        << "*.iiq" << "*.k25" << "*.kdc" << "*.mdc" << "*.mef"
        << "*.mos" << "*.mrw" << "*.nef" << "*.nrw" << "*.obm"
        << "*.orf" << "*.pef" << "*.ptx" << "*.pxn" << "*.r3d"
        << "*.raf" << "*.raw" << "*.rwl" << "*.rw2" << "*.rwz"
        << "*.sr2" << "*.srf" << "*.srw" << "*.tif" << "*.x3f"
        << "*.jpg" << "*.jpeg" << "*.mov" << "*.mp4" << "*.flv"
        << "*.avi" << "*.wmv" << "*.wav" << "*.avchd" << "*.srt";

    QFileInfoList fileList = qdir.entryInfoList(patterns, QDir::Files);

    // Recurse into subdirectories (no dot entries, no symlinks)
    QDir subdirIter(directory);
    const QFileInfoList subdirs = subdirIter.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Readable);
    for (const QFileInfo &subdir : subdirs) {
        fileList << getFileListFromDir(subdir.absoluteFilePath());
    }
    return fileList;
}

// Tint a monochrome glyph (black with alpha) to the given colour, so icons
// stay visible in both light and dark mode.
static QIcon tintedIcon(const QString &resourcePath, const QColor &color)
{
    QPixmap pixmap(resourcePath);
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();
    return QIcon(pixmap);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  m_appStartTimer.start();
  ui->setupUi(this);
  ui->menubar->hide();
  ui->presets->hide();
  updateThemedIcons();

  // The preview label must not claim its pixmap size as minimum size,
  // otherwise the splitter cannot move and the window cannot shrink. The
  // explicit 1x1 minimum overrides the pixmap-based minimumSizeHint;
  // Expanding lets it take the available space (Ignored would let the
  // spacers collapse it to zero height). The pixmap is rescaled to the
  // label in rescalePreview().
  ui->image->setMinimumSize(1, 1);
  ui->image->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  // All extra vertical space in the right pane goes to the preview group
  ui->verticalLayout_2->setStretchFactor(ui->groupBox_2, 1);

  // Labels with long dynamic text (card name, example output path) must not
  // dictate the pane's minimum width; an explicit minimum lets them be
  // clipped instead of blocking the splitter.
  ui->cardLabel->setMinimumWidth(50);
  ui->importToLabel->setMinimumWidth(50);

  // Divider between the file table and the preview/settings pane; the
  // position is restored from the previous session.
  ui->mainSplitter->setStretchFactor(0, 1);
  ui->mainSplitter->setStretchFactor(1, 1);
  connect(ui->mainSplitter, &QSplitter::splitterMoved, this,
          [this](int, int) { rescalePreview(); });
  {
    QSettings uiSettings;
    const QByteArray state = uiSettings.value("mainSplitterState").toByteArray();
    if (!state.isEmpty())
      ui->mainSplitter->restoreState(state);
    const QByteArray geometry = uiSettings.value("mainWindowGeometry").toByteArray();
    if (!geometry.isEmpty())
      restoreGeometry(geometry);
  }

  QSettings settings;
  loadPresetsLocations();
  loadProjectName();
  loadFileNameFormat();

  //    updateImportToLabel();
  setWindowTitle(QString("Quick Import %1").arg(QCoreApplication::applicationVersion()));

  md5Check = settings.value("md5Check", false).toBool();
  ejectAfterImport = settings.value("ejectAfterImport", false).toBool();
  deleteAfterImport = settings.value("deleteAfterImport", false).toBool();
  previewImage = settings.value("previewImage", true).toBool();

  deleteExisting = settings.value("deleteExisting", false).toBool();
  quitEmptyCard = settings.value("quitEmptyCard", false).toBool();
  quitAfterImport = settings.value("quitAfterImport", false).toBool();
  ejectIfEmpty = settings.value("ejectIfEmpty", false).toBool();
  doBackupImport = settings.value("doBackupImport", false).toBool();
  openApplicationAfterImport = settings.value("openApplicationAfterImport", false).toBool();
  ui->OpenApplicationLocation->setEnabled(openApplicationAfterImport);
  ui->openApplicationAfterImport->setCheckState(openApplicationAfterImport ? Qt::Checked
                                                                           : Qt::Unchecked);

  openApplicationLocation = settings.value("openApplicationLocation", "").toString();

  QFileInfo fileInfo(openApplicationLocation);
  QString applicationName = fileInfo.fileName();
  ui->openApplicationText->setText(applicationName);

  ui->deleteAfterImportBox->setCheckState(deleteAfterImport ? Qt::Checked : Qt::Unchecked);
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

  connect(ui->deviceWidget, &deviceList::selectedUpdated, this, &MainWindow::selectedUpdated);
  connect(ui->deviceWidget, &deviceList::spaceButtonPressed, this, &MainWindow::spaceButtonPressed);
  connect(ui->deviceWidget,
          &deviceList::returnButtonPressed,
          this,
          &MainWindow::returnButtonPressed);
  connect(ui->deviceWidget, &deviceList::doneLoading, this, &MainWindow::doneLoadingCard);

  connect(ui->deviceWidget, &deviceList::selectedNode, this, &MainWindow::selectedNode);

  // One persistent preview loader thread for the lifetime of the window
  m_previewThread = new QThread(this);
  m_previewLoader = new imageLoader();
  m_previewLoader->moveToThread(m_previewThread);
  connect(m_previewLoader, &imageLoader::imageLoaded, this, &MainWindow::previewLoaded);
  connect(m_previewThread, &QThread::finished, m_previewLoader, &QObject::deleteLater);
  m_previewThread->start();

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

  const bool showAbout = !settings.value("dontShowAboutDialog", false).toBool();
  QTimer::singleShot(0, this, [this, showAbout]() {
    if (showAbout) {
      showAboutDialog();
    }
    this->raise();
    this->activateWindow();
    this->setFocus(Qt::ActiveWindowFocusReason);
    on_selectCard_clicked();
  });

  // XMPEngine test;
}
void MainWindow::updatePresetList() {
    ui->presetComboBox->clear();

    // Only delete models we created; never delete Qt's internal default model
    QAbstractItemModel *old = ui->presetComboBox->model();
    ui->presetComboBox->setModel(nullptr);
    if (qobject_cast<presetListModel *>(old)) {
        delete old;
    }
    auto *model = new presetListModel(presetList);
    ui->presetComboBox->setModel(model);

    ui->presetComboBox->setPlaceholderText(tr("--Select to load preset--"));
    ui->presetComboBox->setCurrentIndex(-1);
}
void MainWindow::loadPresets() {
  QSettings settings;

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
  QSettings settings;
  // settings.setValue("locationPresets", QStringList());
  // settings.setValue("PresetLocationLastUsed", -1);
  importLocationList = settings.value("locationPresets",  QStringList()).toStringList();
  importBackupLocationList = settings.value("backupLocationPresets",  QStringList()).toStringList();
  int sel = settings.value("PresetLocationLastUsed").toInt();
  qDebug() << "Import Locatiom Sel" << sel;
  resetLocationPreset(sel);
  sel = settings.value("PresetBackupLocationLastUsed").toInt();
  resetBackupLocationPreset(sel);
}
void MainWindow::savePresetsLocations(int sel = -1) {
  QSettings settings;
  settings.setValue("locationPresets", importLocationList);
  settings.setValue("PresetLocationLastUsed", sel);
}
void MainWindow::saveBackupPresetsLocations(int sel = -1)
{
    QSettings settings;
    settings.setValue("backupLocationPresets", importBackupLocationList);
    settings.setValue("PresetBackupLocationLastUsed", sel);
}

void MainWindow::savePresets() {
  qDebug() << "save Presets";
  QSettings settings;

  // Convert QList<presetSetting> to QJsonArray
  QJsonArray jsonArray = presetSettingsToJsonArray(presetList);

  // Convert QJsonArray to QByteArray
  QByteArray byteArray = QJsonDocument(jsonArray).toJson();
  settings.setValue("presetSettings", byteArray);
}

void MainWindow::slotDeviceAdded(const QString &dev) {
  qDebug() << "Device added:" << dev;
  const QString devicePath = QStringLiteral("/dev/") + dev;

  // The disk-appeared event fires when the *device* shows up, usually
  // before macOS has mounted the volume. On startup the watcher also
  // enumerates the disks that are already present — ignore those, the
  // constructor opens the card selection itself.
  if (m_appStartTimer.isValid() && m_appStartTimer.elapsed() < 5000)
    return;
  if (m_pendingInsertedDevices.contains(devicePath))
    return;
  m_pendingInsertedDevices.insert(devicePath);
  waitForVolumeMount(devicePath, 20); // poll up to ~10 s for the mount
}

// Poll until the freshly inserted device is mounted, then ask to open it.
void MainWindow::waitForVolumeMount(const QString &devicePath, int attemptsLeft)
{
    if (!m_pendingInsertedDevices.contains(devicePath))
        return; // device was removed again in the meantime

    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady())
            continue;
        const QString mountedDevice = QString::fromUtf8(storage.device());
        // Match the device itself or one of its partitions (disk4 -> disk4s1)
        if (mountedDevice != devicePath
            && !mountedDevice.startsWith(devicePath + QStringLiteral("s")))
            continue;

        m_pendingInsertedDevices.remove(devicePath);

        if (storage.isReadOnly())
            return;
        const QString fs = storage.fileSystemType().toLower();
        if (!(fs.contains("exfat") || fs.contains("fat") || fs.contains("msdos")))
            return; // not a memory card filesystem
        if (selectedCard.isValid() && selectedCard.rootPath() == storage.rootPath())
            return; // this card is already loaded

        askToOpenInsertedCard();
        return;
    }

    if (attemptsLeft <= 0) {
        // Never mounted (unformatted disk, mount refused, ...) — give up
        m_pendingInsertedDevices.remove(devicePath);
        return;
    }
    QTimer::singleShot(500, this, [this, devicePath, attemptsLeft]() {
        waitForVolumeMount(devicePath, attemptsLeft - 1);
    });
}

void MainWindow::askToOpenInsertedCard()
{
    if (m_insertPromptOpen)
        return; // one question at a time
    m_insertPromptOpen = true;
    const QMessageBox::StandardButton reply
        = QMessageBox::question(this,
                                tr("Card inserted"),
                                tr("Do you want to open the newly inserted card?"),
                                QMessageBox::Yes | QMessageBox::No);
    m_insertPromptOpen = false;
    if (reply == QMessageBox::Yes)
        on_selectCard_clicked();
}

void MainWindow::slotDeviceChanged(const QString &dev) {
  qDebug("change %s", qPrintable(dev));
}

void MainWindow::slotDeviceRemoved(const QString &dev) {
  qDebug() << "Device removed:" << dev;

  QString devicePath = QStringLiteral("/dev/") + dev;
  m_pendingInsertedDevices.remove(devicePath);
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
  {
    QSettings settings;
    settings.setValue("mainSplitterState", ui->mainSplitter->saveState());
    settings.setValue("mainWindowGeometry", saveGeometry());
  }
  if (m_previewThread) {
    m_previewThread->quit();
    m_previewThread->wait();
  }
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
    if (importFolder.size() > 0 && projectName.size() > 0 &&
        fileNameFormat.size() > 0) {
      ui->moveButton->setDisabled(false);
    }

    else {
        ui->moveButton->setDisabled(true);
    }
  }
}

void MainWindow::applyCheckToSelection(CheckAction action) {
  auto *model = qobject_cast<FileInfoModel *>(ui->deviceWidget->model());
  if (!model)
    return;
  QItemSelectionModel *selectionModel = ui->deviceWidget->selectionModel();
  if (!selectionModel)
    return;

  // For Flip, the first item decides the target state for the whole selection
  bool flipTarget = true;
  bool flipTargetSet = false;

  const QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
  for (const QModelIndex &index : selectedIndexes) {
    if (!index.isValid())
      continue;
    const QModelIndex sourceIndex = model->index(index.row(), 0, index.parent());
    if (!sourceIndex.isValid())
      continue;
    TreeNode *node = static_cast<TreeNode *>(sourceIndex.internalPointer());

    bool selected = true;
    switch (action) {
    case CheckAction::Check:
      selected = true;
      break;
    case CheckAction::Uncheck:
      selected = false;
      break;
    case CheckAction::Flip:
      if (!flipTargetSet) {
        flipTargetSet = true;
        flipTarget = !node->isSelected;
      }
      selected = flipTarget;
      break;
    }

    node->isSelected = selected;
    if (!node->isFile) {
      if (selected)
        model->setSelect(node);
      else
        model->setDeselect(node);
    }
  }
  model->refreshChecks();
}

void MainWindow::on_checkSelected_clicked() {
  applyCheckToSelection(CheckAction::Check);
}

void MainWindow::on_uncheckSelected_clicked() {
  applyCheckToSelection(CheckAction::Uncheck);
}

void MainWindow::flipSelectedItems() {
  applyCheckToSelection(CheckAction::Flip);
}

QList<QFileInfo> MainWindow::getFiles(QString map) {
  QList<QFileInfo> files;

  files = getFileListFromDir(map);
  qDebug() << "Found # files:" << files.count();
  return files;
}
void MainWindow::displayNoCardDialog()
{
    QMessageBox msgBox(this);
    msgBox.setText(tr("No Card found, please insert card."));
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.exec();
}

void MainWindow::setBackupUiEnabled(bool enabled)
{
    ui->importBackupLocation->setEnabled(enabled);
    ui->deleteBackupLocationButton->setEnabled(enabled);
    ui->selectBackupLocation->setEnabled(enabled);
    ui->freeSpaceBackupLabel->setEnabled(enabled);
    ui->freeSpaceBackup->setEnabled(enabled);
}

void MainWindow::saveBoolSetting(const QString &key, bool &member, int state)
{
    member = (state == Qt::Checked);
    QSettings settings;
    settings.setValue(key, member);
}

void MainWindow::on_selectCard_clicked() {

  QList<QStorageInfo> cardList;
  ui->deviceWidget->setEnabled(false);

  foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
    if (storage.isReadOnly())
    {
      qDebug() << "isReadOnly:" << storage.isReadOnly();
      continue;
    }

    qDebug() << "name:" << storage.name();
    qDebug() << "fileSystemType:" << storage.fileSystemType();
    qDebug() << "size:" << storage.bytesTotal() / 1000 / 1000 << "MB";
    qDebug() << "availableSize:" << storage.bytesAvailable() / 1000 / 1000
             << "MB";
    QString fs = storage.fileSystemType().toLower();
    if ((fs.contains("exfat") ||
         fs.contains("fat") ||    // fat, fat32, vfat, msdos
         fs.contains("vfat") ||
         fs.contains("msdos")) 
         
        //       && !storage.isReadOnly()
      ) {
      cardList.append(storage);
    }
  }
  if (cardList.count() < 1) {
      displayNoCardDialog();
      return;
  }
  if (cardList.count() == 1) {
      selectedCard = cardList.at(0);
  } else {
      SelectCardDialog window;
      window.setCards(cardList);
      if (window.exec()) {
          selectedCard = window.getSelected();
      }
  }
  reloadCard();
  raise();
  activateWindow();
  setFocus(Qt::ActiveWindowFocusReason);
}
void MainWindow::doneLoadingCard()
{
    ui->deviceWidget->setEnabled(true);
    statusBar()->showMessage(tr("Done loading card."), 5000);
    selectedUpdated(0, 0);
    totalSelectedSize = 0;
}

void MainWindow::updateProcessStatus(QString str)
{
    statusBar()->showMessage(tr("Loading card...") + str);
}

void MainWindow::reloadCard() {
    qDebug() << "Reload Card";
    emptyMainWindow();
    m_lastCardFileCount = 0; // no card = treat as empty

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
        m_lastCardFileCount = files.count();
        ui->deviceWidget->setFiles(files);
        connect(ui->deviceWidget->fileModel,
                &FileInfoModel::updateProcessStatus,
                this,
                &MainWindow::updateProcessStatus,
                Qt::UniqueConnection);
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

  auto state = rawProc.open_file(QFile::encodeName(id).constData());
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

  return thumbnail.scaled(width / 2, height / 2, Qt::KeepAspectRatio);
}

void MainWindow::displayImage(QString rawFilePath, bool window = true,
                              int h = 0, int w = 0) {
  statusBar()->showMessage(tr("Loading image, please wait."));
  QImage image = requestImage(rawFilePath, h, w);
  if (window) {
    BorderlessDialog dialog2(image);
    statusBar()->clearMessage();
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
      delete key_press;
    }
  } else {
    ui->image->setPixmap(QPixmap::fromImage(image));
  }

  // Create a QDialog and set the label as its central widget
}

void MainWindow::emptyMainWindow() {
  ui->deviceWidget->setModel(nullptr);
  // The tree nodes are about to be deleted; drop our pointer and any cached
  // previews of the (possibly re-inserted, modified) card.
  currentSelectedImage = nullptr;
  if (m_previewLoader)
    QMetaObject::invokeMethod(m_previewLoader, &imageLoader::clearCache, Qt::QueuedConnection);
  selectedUpdated(0, 0);
  ui->ejectButton->setDisabled(true);
  ui->reloadButton->setDisabled(true);
  ui->moveButton->setDisabled(true);
  ui->pixmapLabel->setPixmap(QPixmap());
  ui->deviceWidget->setEnabled(false);
  ui->cardLabel->setText(QString(tr("No card loaded.")));
  ui->image->setPixmap(QPixmap());
  m_currentPreviewImage = QImage();
  setBackupUiEnabled(doBackupImport);
}

// SLOT for selection of node on ListWidget
void MainWindow::selectedNode(TreeNode *image) {
  if (!image || !previewImage)
    return;

  if (!image->isFile) {
    // Not a file: preview the first child instead (guard against empty list)
    if (image->children.isEmpty())
      return;
    selectedNode(image->children.first());
    return;
  }

  currentSelectedImage = image;
  updateImportToLabel();

  // The persistent loader coalesces requests (latest wins) and serves
  // repeats from its cache; stale results are dropped in previewLoaded().
  m_previewLoader->requestImage(image->filePath);

  // Prefetch neighbours in the same group so arrow-key browsing is instant
  QStringList prefetch;
  if (TreeNode *parent = image->parent) {
    const int idx = parent->children.indexOf(image);
    for (int offset : {1, -1, 2}) {
      const int i = idx + offset;
      if (i >= 0 && i < parent->children.count()) {
        TreeNode *sibling = parent->children.at(i);
        if (sibling && sibling->isFile)
          prefetch << sibling->filePath;
      }
    }
  }
  m_previewLoader->requestPrefetch(prefetch);
}

void MainWindow::previewLoaded(const QString &path, const QImage &image, bool failed)
{
  // Drop results that no longer match the current selection
  if (!currentSelectedImage || currentSelectedImage->filePath != path)
    return;
  showImage(image, failed);
}

void MainWindow::showImage(const QImage &image, bool failed)
{
    if (!currentSelectedImage) {
        qWarning() << "showImage(): currentSelectedImage is null";
        return;
    }
    // label.resize(image.size());
    QImage img(image);
    QPainter painter(&img);

    // Set font, size, and color
    QFont font("Arial", 30); // You can customize the font and size
    painter.setFont(font);
    // QPoint point(50, 50);
    painter.setPen(QColor(Qt::white)); // You can customize the text color

    // Draw text at the specified position
    const double shutterSpeed = currentSelectedImage->imageInfo.shutterSpeed;
    QString shutterStr;
    if (shutterSpeed >= 1.0) {
        shutterStr = QString("%1s").arg(shutterSpeed, 0, 'f', 1);
    } else if (shutterSpeed > 0.0) {
        shutterStr = QString("1/%1s").arg(qRound(1.0 / shutterSpeed));
    } else {
        shutterStr = QStringLiteral("-");
    }

    painter.drawText(10,
                     10,
                     1024,
                     1024,
                     Qt::AlignLeft,
                     QString("%1\nf %2 - %3\nISO %4\n%5 mm")
                         .arg(currentSelectedImage->info.fileName())
                         .arg(currentSelectedImage->imageInfo.aperture, 0, 'f', 1)
                         .arg(shutterStr)
                         .arg(currentSelectedImage->imageInfo.isoValue)
                         .arg(currentSelectedImage->imageInfo.focalLength)

    );

    painter.drawText(10,
                     10,
                     1004,
                     1004,
                     Qt::AlignRight,
                     QString("%1\n%2\n#%3\n%4 %5")
                         .arg(currentSelectedImage->imageInfo.ownerName,
                              currentSelectedImage->imageInfo.cameraName,
                              currentSelectedImage->imageInfo.serialNumber,
                              currentSelectedImage->imageInfo.lensMake,
                              currentSelectedImage->imageInfo.lensModel)

    );
    if (failed) {
        painter.drawText(0,
                         0,
                         img.width(),
                         img.height(),
                         Qt::AlignCenter,
                         QString(tr("Failed to load image.")));
    }
    // Keep the full-size overlaid image so the preview can rescale when the
    // splitter or the window is resized.
    m_currentPreviewImage = img;
    rescalePreview();
}

void MainWindow::rescalePreview()
{
    if (m_currentPreviewImage.isNull())
        return;
    const QSize target = ui->image->size();
    if (target.width() < 2 || target.height() < 2)
        return;
    ui->image->setPixmap(QPixmap::fromImage(
        m_currentPreviewImage.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

void MainWindow::updateThemedIcons()
{
    ui->reloadButton->setIcon(
        tintedIcon(QStringLiteral(":/icons8-Refresh-64.png"),
                   palette().color(QPalette::ButtonText)));
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    // Re-tint icons when the system switches between light and dark mode
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange) {
        updateThemedIcons();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  rescalePreview();
}

void MainWindow::on_checkAll_clicked() {
  if (auto *model = qobject_cast<FileInfoModel *>(ui->deviceWidget->model()))
    model->selectAll();
}

void MainWindow::on_uncheckAll_clicked() {
  if (auto *model = qobject_cast<FileInfoModel *>(ui->deviceWidget->model()))
    model->deSelectAll();
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
    now = fileCopyWorker::captureTimestamp(currentSelectedImage->info,
                                           currentSelectedImage->imageInfo);
    fileinfo = currentSelectedImage->info;
  }

  QString example =
      fileCopyWorker::processNewFileName(importFolder, projectName, now,
                                         imageInfo, fileinfo, fileNameFormat)
          .at(2);
  ui->importToLabel->setText(example);

  // QStorageInfo does real disk I/O and this function runs on every
  // keystroke; refresh the free-space values at most every few seconds
  // (or when the folders/backup setting change).
  const bool refreshFreeSpace = !m_freeSpaceTimer.isValid()
                                || m_freeSpaceTimer.elapsed() > 3000
                                || importFolder != m_freeSpaceFolder
                                || importBackupFolder != m_freeSpaceBackupFolder
                                || doBackupImport != m_freeSpaceBackupEnabled;
  // Free space of the volume the folder lives on. When the folder itself
  // does not exist yet, walk up to the nearest existing parent — that is
  // where mkpath will create it, so its volume is the right one (and
  // QStorageInfo on a missing path would report -1 → "-0.00 GB").
  auto availableForPath = [](const QString &path) -> qint64 {
    QString probe = QDir::cleanPath(path);
    while (!probe.isEmpty() && !QFileInfo::exists(probe)) {
      const int idx = probe.lastIndexOf(QLatin1Char('/'));
      if (idx <= 0) {
        probe = QStringLiteral("/");
        break;
      }
      probe = probe.left(idx);
    }
    return QStorageInfo(probe).bytesAvailable();
  };

  if (refreshFreeSpace) {
    m_freeSpaceTimer.start();
    m_freeSpaceFolder = importFolder;
    m_freeSpaceBackupFolder = importBackupFolder;
    m_freeSpaceBackupEnabled = doBackupImport;
    freeProjectSpace = availableForPath(importFolder);
    m_freeBackupSpace = doBackupImport ? availableForPath(importBackupFolder) : -1;
  }

  const bool importFolderMissing = !importFolder.isEmpty() && !QDir(importFolder).exists();
  QString freeText = QString("%1 GB").arg(
      ((float)freeProjectSpace / 1000 / 1000 / 1000), 0, 'f', 2);
  if (importFolderMissing)
      freeText += tr(" (new folder)");
  ui->freeDiskSpace->setText(freeText);

  if (doBackupImport) {
      QString backupText = QString("%1 GB").arg(((float) m_freeBackupSpace / 1000 / 1000 / 1000),
                                                0, 'f', 2);
      if (!importBackupFolder.isEmpty() && !QDir(importBackupFolder).exists())
          backupText += tr(" (new folder)");
      ui->freeSpaceBackup->setText(backupText);
  } else {
      ui->freeSpaceBackup->setText("");
  }

  if (totalSelectedSize <= 0)
    ui->moveButton->setDisabled(true);
  else {
    if (importFolder.size() > 0 && projectName.size() > 0 &&
        fileNameFormat.size() > 0) {
      ui->moveButton->setDisabled(false);
    }

    else {
      ui->moveButton->setDisabled(true);
    }
  }
}

// Ask to create a missing destination folder; returns false when the user
// declines or creation fails.
bool MainWindow::ensureFolderExists(const QString &folder, const QString &description)
{
    if (folder.isEmpty() || QDir(folder).exists())
        return !folder.isEmpty();

    const QMessageBox::StandardButton reply
        = QMessageBox::question(this,
                                tr("Folder does not exist"),
                                tr("The %1 does not exist:\n%2\n\nCreate it?")
                                    .arg(description, folder),
                                QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return false;

    if (!QDir().mkpath(folder)) {
        QMessageBox msgBox(this);
        msgBox.setText(tr("Could not create folder %1").arg(folder));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
        return false;
    }
    // Refresh the free-space labels now that the folder exists
    m_freeSpaceTimer.invalidate();
    updateImportToLabel();
    return true;
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
    if (!ensureFolderExists(importFolder, tr("import folder")))
        return;
    if (doBackupImport && !ensureFolderExists(importBackupFolder, tr("backup folder")))
        return;
    if (freeProjectSpace < totalSelectedSize) {
        QMessageBox msgBox;
        msgBox.setText(tr("Not enough diskspace available on project location!"));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
        return;
    }
    if (doBackupImport) {
        if (importBackupFolder.isEmpty()) {
            QMessageBox msgBox;
            msgBox.setText(tr("No backup folder set, please set one first."));
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();
            return;
        }

        QStorageInfo backupInfo(importBackupFolder);
        if (!backupInfo.isValid() || !backupInfo.isReady()) {
            QMessageBox msgBox;
            msgBox.setText(tr("Backup location is not available."));
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();
            return;
        }

        if (backupInfo.bytesAvailable() < totalSelectedSize) {
            QMessageBox msgBox;
            msgBox.setText(tr("Not enough diskspace available on backup location!"));
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();
            return;
        }
    }

    if (auto *model = qobject_cast<FileInfoModel *>(ui->deviceWidget->model())) {
        QList<fileInfoStruct> list;
        list = model->getSelectedFiles();

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

        if (ejectAfterImport && ok) {
            qDebug() << "Eject after move";
            on_ejectButton_clicked();
        }

        // Only open the external application when something was actually
        // imported (a re-import where every file already existed copies
        // nothing, so there is nothing new to review).
        if (openApplicationAfterImport && ok && dialog.copiedCount() > 0) {
            QString path = dialog.getLastFilePath();
            QStringList arguments;
            arguments << path;
            QProcess::startDetached(openApplicationLocation, arguments);
        }

        if (quitAfterImport && ok) {
            qApp->quit();
        }

        // reloadCard() rescans the card synchronously and updates
        // m_lastCardFileCount; the model's rowCount() is NOT usable here
        // because the tree is built asynchronously (it reads 0 until done,
        // which used to quit/eject with a full card).
        reloadCard();
        if (ejectIfEmpty && ok && m_lastCardFileCount <= 0) {
            doEject();
        }

        if (quitEmptyCard && ok && m_lastCardFileCount <= 0) {
            qApp->quit();
        }
    }
}

int MainWindow::doEject() {
#if defined(Q_OS_MACOS)
  QProcess process;
  process.start("diskutil", {"unmountDisk", QString::fromUtf8(selectedCard.device())});
  process.waitForFinished();
  return process.exitCode();
#else
  // Ejecting is not implemented for this platform yet
  qWarning() << "Eject is only supported on macOS";
  return -1;
#endif
}

void MainWindow::returnButtonPressed() { on_quickViewButton_clicked(); }

void MainWindow::spaceButtonPressed() {
  flipSelectedItems();
  // on_checkSelected_clicked();
}

void MainWindow::on_quickViewButton_clicked() {
  if (ui->deviceWidget->model()) {
    QItemSelectionModel *selectionModel = ui->deviceWidget->selectionModel();
    if (selectionModel) {
      const QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
      if (selectedIndexes.isEmpty()) {
        return;
      }
      const QModelIndex index = selectedIndexes.at(0);
      if (index.isValid()) {
        // Map the index to get the corresponding QFileInfo
        QModelIndex sourceIndex =
            ui->deviceWidget->model()->index(index.row(), 0, index.parent());
        if (sourceIndex.isValid()) {
          TreeNode *node =
              static_cast<TreeNode *>(sourceIndex.internalPointer());

          if (node->isFile) {
            displayImage(node->info.absoluteFilePath(),0,0);
          } else {
            if (node->children.isEmpty())
              return;
            TreeNode *child = node->children.at(0);
            if (child && child->isFile) {
              displayImage(child->info.absoluteFilePath(), true,0,0);
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
  saveBoolSetting("previewImage", previewImage, arg1);
  if (!previewImage)
    ui->image->setPixmap(QPixmap());
}

void MainWindow::on_reloadButton_clicked() { reloadCard(); }

void MainWindow::on_mdCheckBox_stateChanged(int arg1) {
  saveBoolSetting("md5Check", md5Check, arg1);
}

void MainWindow::on_deleteAfterImportBox_stateChanged(int arg1) {
  saveBoolSetting("deleteAfterImport", deleteAfterImport, arg1);
}

void MainWindow::on_ejectBox_stateChanged(int arg1) {
  saveBoolSetting("ejectAfterImport", ejectAfterImport, arg1);
}

void MainWindow::on_deleteExistingBox_stateChanged(int arg1) {
  saveBoolSetting("deleteExisting", deleteExisting, arg1);
}

void MainWindow::on_quitEmptyCardBox_stateChanged(int arg1) {
  saveBoolSetting("quitEmptyCard", quitEmptyCard, arg1);
}

void MainWindow::on_ejectIfEmptyBox_stateChanged(int arg1) {
  saveBoolSetting("ejectIfEmpty", ejectIfEmpty, arg1);
}

void MainWindow::on_quitAfterImportBox_stateChanged(int arg1) {
  saveBoolSetting("quitAfterImport", quitAfterImport, arg1);
}

void MainWindow::on_toolButton_clicked() {
  presetDialog preset(this);
  preset.exec();
}

void MainWindow::on_presetComboBox_activated(int index) {
  if (index >= 0 && index < presetList.count()) {
    presetSetting nw = presetList.at(index);
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
    Q_UNUSED(sel);
    ui->importBackupLocation->clear();
    ui->importBackupLocation->addItems(importBackupLocationList);
    if (importBackupLocationList.count() <= 0)
        ui->importBackupLocation->setPlaceholderText(
            tr("--Back-up location not set--"));

    if (sel >= 0 && ui->importBackupLocation->count() > 0) {
        ui->importBackupLocation->setCurrentIndex(sel);
        importBackupFolder = importBackupLocationList.at(sel);
    } else {
        // sel = 0;
        importBackupFolder = QString();
    }

    updateImportToLabel();
}
void MainWindow::resetLocationPreset(int sel = -1)
{
    ui->importLocation->clear();
    ui->importLocation->addItems(importLocationList);
    if (importLocationList.count() <= 0)
        ui->importLocation->setPlaceholderText(tr("--Location not set--"));

    if (sel >= 0 && ui->importLocation->count() > 0) {
        ui->importLocation->setCurrentIndex(sel);
        importFolder = importLocationList.at(sel);
    } else {
        // sel = 0;
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
    QSettings settings;
    settings.setValue("Import Folder", importFolder);
    addLocationPreset(importFolder);

    updateImportToLabel();
  }
}

void MainWindow::on_importLocation_activated(int index) {
  qDebug() << index;
  importFolder = importLocationList.at(index);
  savePresetsLocations(index);
  updateImportToLabel();
}

void MainWindow::on_saveProjectNameButton_clicked()
{
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
  QSettings settings;
  settings.setValue("projectNameList", projectNameList);
  settings.setValue("projectNameLastUsed", sel);
}

void MainWindow::loadProjectName() {
  QSettings settings;
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
        tr("-- set project name --"));
    sel = -1;
  }

  if (sel >= 0) {
    ui->projectName->setCurrentIndex(sel);
    projectName = projectNameList.at(sel);
  } else {
    projectName = QString();
    // sel = 0;
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
  resetFileNameFormat(sel);
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
  resetFileNameFormat(sel);
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
  QSettings settings;
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

  resetFileNameFormat(sel);
}
void MainWindow::resetFileNameFormat(int sel = -1) {
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
    if (!fileNameFormatList.contains(fileNameFormat))
      fileNameFormatList.append(fileNameFormat);
    ui->fileNameFormat->clear();
    ui->fileNameFormat->addItems(fileNameFormatList);
    ui->fileNameFormat->setCurrentIndex(0);
  }

  updateImportToLabel();
}
void MainWindow::saveFileNameFormat(int sel) {
  qDebug() << "FileNameFormat Save" << fileNameFormatList;
  QSettings settings;
  settings.setValue("fileNameFormat", fileNameFormatList);
  settings.setValue("fileNameFormatLastUsed", sel);
}

void MainWindow::on_shortcutDialogButton_clicked() {
  if (shortcutDialogWindow == nullptr) {
    shortcutDialogWindow = new shortcutDialog();
    shortcutDialogWindow->show();
    connect(shortcutDialogWindow, &shortcutDialog::finished, this,
            &MainWindow::shortcutWindowFinished);
  } else {
    shortcutDialogWindow->hide();
    delete shortcutDialogWindow;
    shortcutDialogWindow = nullptr;
  }
}
void MainWindow::shortcutWindowFinished(int) {
  if (!shortcutDialogWindow)
    return;
  shortcutDialogWindow->hide();
  delete shortcutDialogWindow;
  shortcutDialogWindow = nullptr;
}

void MainWindow::on_backupBox_stateChanged(int arg1) {
  saveBoolSetting("doBackupImport", doBackupImport, arg1);
  setBackupUiEnabled(doBackupImport);
  updateImportToLabel();
}

void MainWindow::on_selectBackupLocation_clicked()
{
    QString directory = QFileDialog::getExistingDirectory(this,
                                                          tr("Select a directory"),
                                                          importFolder);
    if (!directory.isEmpty()) {
        importBackupFolder = directory;
        //QSettings settings;
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

void MainWindow::on_OpenApplicationLocation_clicked()
{
    QString filter;
    QString defaultPath;

    if (QSysInfo::productType() == "windows") {
        filter = tr("Applications (*.exe)");
        defaultPath = "C:\\Program Files";
    } else if (QSysInfo::productType() == "macos") {
        filter = tr("Applications (*.app)");
        defaultPath = "/Applications";
    } else { // Assuming Linux or other Unix-like OS
        filter = tr("Applications (*.sh *.bin *.run *.AppImage);;All Files (*)");
        defaultPath = "/usr/bin";
    }

    QString app = QFileDialog::getOpenFileName(this, tr("Choose Application"), defaultPath, filter);

    if (!app.isEmpty()) {
        QFileInfo fileInfo(app);
        QString applicationName = fileInfo.fileName();

        openApplicationLocation = app;
        QSettings settings;
        settings.setValue("openApplicationLocation", openApplicationLocation);
        ui->openApplicationText->setText(applicationName);
    }
}

void MainWindow::on_openApplicationAfterImport_stateChanged(int arg1)
{
    saveBoolSetting("openApplicationAfterImport", openApplicationAfterImport, arg1);
    ui->OpenApplicationLocation->setEnabled(openApplicationAfterImport);
}
