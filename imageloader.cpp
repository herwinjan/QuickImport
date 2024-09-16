#include "imageloader.h"

#include <QDebug>
#include <QImage>
#include <QImageReader>

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
    QImage thumbnail;

    if (node->isFile) {
        QList<QByteArray> items = QImageReader::supportedImageFormats();
        if (items.indexOf(node->info.suffix().toLower().toLocal8Bit()) >= 0) {
            thumbnail = QImage(node->filePath);

            thumbnail = thumbnail.scaled(1024, 1024, Qt::KeepAspectRatio);
        } else {
            rawProc = new LibRaw();
            qDebug() << "LibRaw: " << rawProc->versionNumber() << rawProc->version();
            auto state = rawProc->open_file(node->filePath.toLatin1().data());
            qDebug() << "open" << node->filePath.toLatin1().data() << state;
            if (LIBRAW_SUCCESS != state) {
                QImage img = QImage(1024, 682, QImage::Format_RGB32);
                img.fill(Qt::black);
                img = img.scaled(1024, 682, Qt::KeepAspectRatio);
                // emit loadingFailed();
                emit imageLoaded(img, true);
                emit finished();
                return;
            }

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
        }
    }
    // Check if the image was loaded successfully
    if (thumbnail.isNull()) {
        qDebug() << "Failed loading thumb" << rawProc->imgdata.shootinginfo.BodySerial;
        emit loadingFailed();
    } else {
        emit imageLoaded(thumbnail);
    }

    emit finished();
}
