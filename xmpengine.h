#ifndef XMPENGINE_H
#define XMPENGINE_H

#include <QObject>
#include "exiv2/exiv2.hpp"
#include <fstream>
#include <iostream>

class XMPEngine : public QObject
{
    Q_OBJECT
private:
    Exiv2::XmpData xmpData;
    QString fileName;

public:
    explicit XMPEngine();

    void writeXmpSidecar(const std::string &filePath);

    void openXmpFile(QString);
    void saveXmpFile();
    void dumpXmp();
    void addLangAlt(QString key, QString value, bool _overwrite = false);
    void addXmpBag(QString key, QString value, bool _overwrite = false);
    void appendXmpBag(QString key, QString value);
};

#endif // XMPENGINE_H
