#include "imageloader.h"

#include <QDebug>
#include <QImage>
#include <libraw/libraw.h>

imageLoader::imageLoader(QObject *parent)
    : QObject(parent)

{
    qDebug() << "load Image()";
}
void imageLoader::loadImageFile(const QString &imagePath)

{
    qDebug() << "load Image Init" << imagePath;
    m_imagePath = imagePath;
}

void imageLoader::loadImage()
{
    qDebug() << "start";
    LibRaw *rawProc = new LibRaw();
    qDebug() << "load Image nu" << m_imagePath;
    auto state = rawProc->open_file(m_imagePath.toLatin1().data());

    QImage thumbnail;

    if (LIBRAW_SUCCESS == state) {
        if (LIBRAW_SUCCESS == rawProc->unpack_thumb()) {
            if (LIBRAW_THUMBNAIL_JPEG == rawProc->imgdata.thumbnail.tformat) {
                thumbnail.loadFromData((unsigned char *) rawProc->imgdata.thumbnail.thumb,
                                       rawProc->imgdata.thumbnail.tlength,
                                       "JPEG");
            }
        }
        rawProc->recycle();
    }
    delete rawProc;

    // Check if the image was loaded successfully
    if (thumbnail.isNull()) {
        emit loadingFailed();
    } else {
        emit imageLoaded(thumbnail);
    }
    qDebug() << "finshed!";
    emit finished();
}
