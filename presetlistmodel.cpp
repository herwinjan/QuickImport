#include "presetlistmodel.h"

#include <QJsonObject>

// Convert a QList<presetSetting> to a QJsonArray

// Convert a QJsonArray to a QList<presetSetting>
QJsonArray presetSettingsToJsonArray(const QList<presetSetting> &settings)
{
    QJsonArray jsonArray;
    for (const auto &setting : settings) {
        QJsonObject jsonObj;
        jsonObj["name"] = setting.name;
        jsonObj["importFolder"] = setting.importFolder;
        jsonObj["md5Check"] = setting.md5Check;
        jsonObj["deleteAfterImport"] = setting.deleteAfterImport;
        jsonObj["ejectAfterImport"] = setting.ejectAfterImport;
        jsonObj["previewImage"] = setting.previewImage;
        jsonObj["deleteExisting"] = setting.deleteExisting;
        jsonObj["quitEmptyCard"] = setting.quitEmptyCard;
        jsonObj["ejectIfEmpty"] = setting.ejectIfEmpty;
        jsonObj["quitAfterImport"] = setting.quitAfterImport;
        jsonArray.append(jsonObj);
    }
    return jsonArray;
}

QList<presetSetting> jsonArrayToPresetSettings(const QJsonArray &jsonArray)
{
    QList<presetSetting> settings;
    for (const auto &jsonValue : jsonArray) {
        const auto jsonObj = jsonValue.toObject();
        settings.append({jsonObj["name"].toString(),
                         jsonObj["importFolder"].toString(),
                         jsonObj["md5Check"].toBool(),
                         jsonObj["deleteAfterImport"].toBool(),
                         jsonObj["ejectAfterImport"].toBool(),
                         jsonObj["previewImage"].toBool(),
                         jsonObj["deleteExisting"].toBool(),
                         jsonObj["quitEmptyCard"].toBool(),
                         jsonObj["ejectIfEmpty"].toBool(),
                         jsonObj["quitAfterImport"].toBool()});
    }
    return settings;
}

presetListModel::presetListModel(const QList<presetSetting> &data, QObject *parent)
    : QAbstractListModel(parent)
    , data_(data)
{}

int presetListModel::rowCount(const QModelIndex &parent) const
{
    return data_.size();
}

QVariant presetListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    return data_[index.row()].name; // assuming name for display
}
