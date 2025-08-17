#ifndef EXTERNALDRIVEICONFETCHER_H
#define EXTERNALDRIVEICONFETCHER_H

#include <QString>
#include <QIcon>

class ExternalDriveIconFetcher
{
public:
    static QIcon getExternalDriveIcon(const QString &drivePath);
    static QPixmap getExternalDrivePixmap(const QString &drivePath);
};

#endif // EXTERNALDRIVEICONFETCHER_H
