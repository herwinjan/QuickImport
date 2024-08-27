#include "imageloader.h"

#include <QDebug>
#include <QImage>

#include <libraw/libraw.h>

imageLoader::imageLoader(QObject *parent)
    : QObject(parent)

{}
void imageLoader::loadImageFile(const TreeNode *_node)

{
    node = _node;
}

void imageLoader::loadImage()
{
    LibRaw *rawProc = NULL;
    bool del = false;

        rawProc = new LibRaw();
        qDebug() << rawProc->cameraCount();
        auto state = rawProc->open_file(m_imagePath.toLatin1().data());
        qDebug() << "open" << state;
        if (LIBRAW_SUCCESS != state) {
            QImage img = QImage(1024, 682, QImage::Format_RGB32);
            img.fill(Qt::black);
            img = img.scaled(1024, 682, Qt::KeepAspectRatio);
            // emit loadingFailed();
            emit imageLoaded(img, true);
            emit finished();
            return;
        }

    QImage thumbnail;

    if (!(rawProc->imgdata.thumbnail.thumb)) {
        qDebug() << "thumb!";
        rawProc->unpack_thumb();
    }

    qDebug() << rawProc->imgdata.thumbnail.tformat;
    if (LIBRAW_THUMBNAIL_JPEG == rawProc->imgdata.thumbnail.tformat) {
        thumbnail.loadFromData((unsigned char *) rawProc->imgdata.thumbnail.thumb,
                               rawProc->imgdata.thumbnail.tlength,
                               "JPEG");
    }
    thumbnail = thumbnail.scaled(1024, 1024, Qt::KeepAspectRatio);

    //    rawProc->recycle();
    delete rawProc;

    // Check if the image was loaded successfully
    if (thumbnail.isNull()) {
        qDebug() << "Failed loading thumb" << rawProc->imgdata.shootinginfo.BodySerial;
        emit loadingFailed();
    } else {
        emit imageLoaded(thumbnail);
    }

    emit finished();
}
