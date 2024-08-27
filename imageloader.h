#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include <QObject>

#include "filelistmodel.h"

class imageLoader : public QObject
{
    Q_OBJECT
public:
    explicit imageLoader(QObject *parent = nullptr);

    void loadImageFile(const TreeNode *_node);
    const TreeNode *node;

public slots:
    void loadImage();
signals:
    void imageLoaded(const QImage &image, bool failed = false);
    void loadingFailed();
    void finished();

private:
    QString m_imagePath;
};

#endif // IMAGELOADER_H
