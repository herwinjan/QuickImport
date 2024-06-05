#ifndef PRESETLISTMODEL_H
#define PRESETLISTMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QObject>

struct presetSetting
{
    QString name;
    QString importFolder;
    bool md5Check;
    bool deleteAfterImport;
    bool ejectAfterImport;
    bool previewImage;
    bool deleteExisting;
    bool quitEmptyCard;
    bool ejectIfEmpty;
    bool quitAfterImport;
};

QJsonArray presetSettingsToJsonArray(const QList<presetSetting> &settings);
QList<presetSetting> jsonArrayToPresetSettings(const QJsonArray &jsonArray);

// Custom model class
class presetListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit presetListModel(const QList<presetSetting> &data, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    QList<presetSetting> data_;
};

#endif // PRESETLISTMODEL_H
