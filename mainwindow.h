#pragma once


#include "fileinfomodel.h"
#include "imageloader.h"
#include "presetlistmodel.h"
#include "shortcutdialog.h"

#include <QElapsedTimer>
#include <QFileInfoList>
#include <QMainWindow>
#include <QSet>
#include <QShortcut>
#include <QStorageInfo>
#include <QThread>

#include "qdevicewatcher/qdevicewatcher.h"

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

    QList<presetSetting> presetList;

    void updatePresetList();
public slots:
    void slotDeviceAdded(const QString &dev);

    void slotDeviceChanged(const QString &dev);

    void slotDeviceRemoved(const QString &dev);

    void doneLoadingCard();
    void updateProcessStatus(QString);

protected:
    void resizeEvent(QResizeEvent *event);

    int doEject();

    shortcutDialog *shortcutDialogWindow = nullptr;

public:
    //TODO store all settings in own class
    QString importFolder;
    QString importBackupFolder;
    QString projectName;
    QString fileNameFormat;
    QStorageInfo selectedCard;
    bool md5Check;
    bool deleteAfterImport;
    bool ejectAfterImport;
    bool previewImage;
    qint64 totalSelectedSize = 0;
    qint64 freeProjectSpace = 0;
    Ui::MainWindow *ui;
    void emptyMainWindow();
    imageLoader *m_previewLoader = nullptr;
    QThread *m_previewThread = nullptr;
    bool doBackupImport = false;

    bool deleteExisting;

    bool quitEmptyCard;
    bool ejectIfEmpty;
    bool quitAfterImport;
    QString openApplicationLocation;
    bool openApplicationAfterImport;

    QStringList importLocationList;
    QStringList importBackupLocationList;
    QStringList projectNameList;
    QStringList fileNameFormatList;

    TreeNode *currentSelectedImage = NULL;

    void loadPresets();
    void savePresets();
    void addLocationPreset(QString location);
    void resetLocationPreset(int sel);
    void addBackupLocationPreset(QString location);
    void resetBackupLocationPreset(int sel);
    void loadPresetsLocations();
    void savePresetsLocations(int sel);
    void saveProjectName(int sel);
    void loadProjectName();
    void resetProjectName(int sel);
    void loadFileNameFormat();
    void resetFileNameFormat(int sel);
    void saveFileNameFormat(int sel = -1);
    void saveBackupPresetsLocations(int sel);
private slots:
    void showImage(const QImage &image, bool failed = false);
    void selectedNode(TreeNode *);
    void showAboutDialog();
    void on_checkSelected_clicked();
    void on_uncheckSelected_clicked();
    void selectedUpdated(int, qint64);
    void on_selectCard_clicked();
    void on_checkAll_clicked();
    void on_uncheckAll_clicked();

    void on_moveButton_clicked();
    void on_mdCheckBox_stateChanged(int arg1);
    void on_deleteAfterImportBox_stateChanged(int arg1);
    void on_ejectBox_stateChanged(int arg1);
    void on_quickViewButton_clicked();
    void spaceButtonPressed();
    void returnButtonPressed();
    void previewLoaded(const QString &path, const QImage &image, bool failed);

    void on_ejectButton_clicked();

    void on_previewImageCheckBox_stateChanged(int arg1);
    void on_reloadButton_clicked();
    void on_deleteExistingBox_stateChanged(int arg1);
    void on_quitEmptyCardBox_stateChanged(int arg1);
    void on_ejectIfEmptyBox_stateChanged(int arg1);

    void on_quitAfterImportBox_stateChanged(int arg1);
    void on_toolButton_clicked();
    void on_presetComboBox_activated(int index);
    void on_projectName_currentTextChanged(const QString &arg1);
    void on_selectImportLocation_clicked();

    void on_importLocation_activated(int index);
    void on_saveProjectNameButton_clicked();
    void on_deleteProjectName_clicked();
    void on_safeFileNameFormat_clicked();
    void on_deleteFileNameFormat_clicked();
    void on_fileNameFormat_currentIndexChanged(int index);
    void on_fileNameFormat_currentTextChanged(const QString &arg1);

    void on_deleteLocationButton_clicked();
    void on_shortcutDialogButton_clicked();
    void shortcutWindowFinished(int);
    void on_backupBox_stateChanged(int arg1);
    void on_selectBackupLocation_clicked();
    void on_deleteBackupLocationButton_clicked();
    void on_fileNameFormat_activated(int index);
    void on_projectName_activated(int index);
    void on_OpenApplicationLocation_clicked();
    void on_openApplicationAfterImport_stateChanged(int arg1);

private:
    void displayNoCardDialog();

    // Persist a checkbox-backed bool setting and update the member
    void saveBoolSetting(const QString &key, bool &member, int state);
    // Enable/disable the backup location controls as one group
    void setBackupUiEnabled(bool enabled);
    // Shared implementation of check/uncheck/flip on the view selection
    enum class CheckAction { Check, Uncheck, Flip };
    void applyCheckToSelection(CheckAction action);

    // Free-space labels: QStorageInfo does real disk I/O, so cache the
    // values briefly instead of querying on every keystroke
    QElapsedTimer m_freeSpaceTimer;
    QString m_freeSpaceFolder;
    QString m_freeSpaceBackupFolder;
    bool m_freeSpaceBackupEnabled = false;
    qint64 m_freeBackupSpace = -1;

    // Card-insertion detection: the disk event fires before the volume is
    // mounted, so we poll for the mount before asking to open the card
    void waitForVolumeMount(const QString &devicePath, int attemptsLeft);
    void askToOpenInsertedCard();
    QSet<QString> m_pendingInsertedDevices;
    bool m_insertPromptOpen = false;
    QElapsedTimer m_appStartTimer; // suppress the startup device enumeration

    // File count from the last synchronous card scan. The model's
    // rowCount() is 0 until the async tree build finishes, so the
    // quit/eject-when-empty decisions must use this instead.
    int m_lastCardFileCount = 0;
};

