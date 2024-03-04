#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include <QObject>

class imageLoader : public QObject
{
    Q_OBJECT
public:
    explicit imageLoader(QObject *parent = nullptr);

    void loadImageFile(const QString &imagePath);

public slots:
    void loadImage();
signals:
    void imageLoaded(const QImage &image);
    void loadingFailed();
    void finished();

private:
    QString m_imagePath;
};

#endif // IMAGELOADER_H
