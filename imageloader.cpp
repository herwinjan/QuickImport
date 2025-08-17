#include "imageloader.h"

#include <QDebug>
#include <QImage>
#include <QImageReader>
#include <memory>
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
    LibRaw *rawProc = nullptr;
    QImage thumbnail;

    // safety: ensure node is valid
    if (!node) {
        QImage img(1024, 682, QImage::Format_RGB32);
        img.fill(Qt::black);
        emit imageLoaded(img, true);
        emit loadingFailed();
        emit finished();
        return;
    }

    if (node->isFile) {
        QList<QByteArray> items = QImageReader::supportedImageFormats();
        if (items.indexOf(node->info.suffix().toLower().toLocal8Bit()) >= 0) {
            thumbnail = QImage(node->filePath);
            thumbnail = thumbnail.scaled(1024, 1024, Qt::KeepAspectRatio);
        } else {
            std::unique_ptr<LibRaw> rawProc(new LibRaw());
            // keep path bytes alive for the call
            QByteArray path = node->filePath.toLocal8Bit();
            auto state = rawProc->open_file(path.constData());
            if (LIBRAW_SUCCESS != state) {
                // clean up and emit failure
                
                rawProc.reset();
                QImage img(1024, 682, QImage::Format_RGB32);
                img.fill(Qt::black);
                img = img.scaled(1024, 682, Qt::KeepAspectRatio);
                emit imageLoaded(img, true);
                emit loadingFailed();
                emit finished();
                return;
            }

            // try to ensure a thumbnail is available
            if (!(rawProc->imgdata.thumbnail.thumb)) {
                int ret = rawProc->unpack_thumb();
                Q_UNUSED(ret);
                // optionally check ret != LIBRAW_SUCCESS and handle
            }

            // load thumbnail data safely into a QByteArray so we can delete rawProc before using it
            if (LIBRAW_THUMBNAIL_JPEG == rawProc->imgdata.thumbnail.tformat
                && rawProc->imgdata.thumbnail.thumb
                && rawProc->imgdata.thumbnail.tlength > 0) {
                QByteArray thumbData(reinterpret_cast<const char*>(rawProc->imgdata.thumbnail.thumb),
                                     static_cast<int>(rawProc->imgdata.thumbnail.tlength));
                thumbnail.loadFromData(reinterpret_cast<const uchar*>(thumbData.constData()),
                                       thumbData.size(),
                                       "JPEG");
            }

            // free libraw object before further processing
            rawProc.reset();
            

            if (!thumbnail.isNull())
                thumbnail = thumbnail.scaled(1024, 1024, Qt::KeepAspectRatio);
        }
    }

    // Check if the image was loaded successfully
    if (thumbnail.isNull()) {
        QImage img(1024, 682, QImage::Format_RGB32);
        img.fill(Qt::black);
        img = img.scaled(1024, 682, Qt::KeepAspectRatio);
        emit imageLoaded(img, true);
        emit loadingFailed();
    } else {
        emit imageLoaded(thumbnail);
    }

    emit finished();
}
