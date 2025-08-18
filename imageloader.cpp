#include "imageloader.h"

#include <QDebug>
#include <QImage>
#include <QImageReader>
#include <memory>
#include <libraw/libraw.h>
#include <QBuffer>

imageLoader::imageLoader(QObject *parent)
    : QObject(parent)

{}
void imageLoader::loadImageFile(const TreeNode *_node)

{
    node = _node;
}
void imageLoader::loadImage()
{
    QImage thumbnail;
    constexpr int kMaxDim = 1024;

    // safety: ensure node is valid
    if (!node) {
        QImage img(kMaxDim, (kMaxDim * 2) / 3, QImage::Format_RGB32);
        img.fill(Qt::black);
        emit imageLoaded(img, true);
        emit loadingFailed();
        emit finished();
        return;
    }

    if (node->isFile) {
        QList<QByteArray> items = QImageReader::supportedImageFormats();
        if (items.indexOf(node->info.suffix().toLower().toLocal8Bit()) >= 0) {
            QImageReader reader(node->filePath);
            reader.setAutoTransform(true); // honor EXIF orientation
            // Read at most kMaxDim in either dimension to save memory/time
            const QSize origSize = reader.size();
            if (origSize.isValid()) {
                QSize target = origSize;
                target.scale(kMaxDim, kMaxDim, Qt::KeepAspectRatio);
                if (target != origSize)
                    reader.setScaledSize(target);
            }
            if (!reader.read(&thumbnail)) {
                // Fallback: try basic QImage load
                QImage img(node->filePath);
                if (!img.isNull()) {
                    thumbnail = img.scaled(kMaxDim, kMaxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
            }
        } else {
            std::unique_ptr<LibRaw> rawProc(new LibRaw());
            // keep path bytes alive for the call
            QByteArray path = node->filePath.toLocal8Bit();
            auto state = rawProc->open_file(path.constData());
            if (LIBRAW_SUCCESS != state) {
                // clean up and emit failure
                
                rawProc.reset();
                QImage img(kMaxDim, (kMaxDim * 2) / 3, QImage::Format_RGB32);
                img.fill(Qt::black);
                emit imageLoaded(img, true);
                emit loadingFailed();
                emit finished();
                return;
            }

            // Ensure a thumbnail is available
            int ret = LIBRAW_SUCCESS;
            if (!rawProc->imgdata.thumbnail.thumb) {
                ret = rawProc->unpack_thumb();
            }

            if (ret == LIBRAW_SUCCESS && rawProc->imgdata.thumbnail.thumb && rawProc->imgdata.thumbnail.tlength > 0) {
                QByteArray thumbData(reinterpret_cast<const char*>(rawProc->imgdata.thumbnail.thumb),
                                     static_cast<int>(rawProc->imgdata.thumbnail.tlength));
                QBuffer buf(&thumbData);
                buf.open(QIODevice::ReadOnly);
                QImageReader r(&buf);
                r.setDecideFormatFromContent(true);
                r.setAutoTransform(true);
                if (!r.read(&thumbnail)) {
                    // Fallback to direct loadFromData
                    QImage tmp;
                    if (tmp.loadFromData(reinterpret_cast<const uchar*>(thumbData.constData()), thumbData.size())) {
                        thumbnail = tmp;
                    }
                }
            }

            // free libraw object before further processing
            rawProc.reset();

            if (!thumbnail.isNull()) {
                thumbnail = thumbnail.scaled(kMaxDim, kMaxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
    }

    // Check if the image was loaded successfully
    if (thumbnail.isNull()) {
        QImage img(kMaxDim, (kMaxDim * 2) / 3, QImage::Format_RGB32);
        img.fill(Qt::black);
        emit imageLoaded(img, true);
        emit loadingFailed();
    } else {
        if (thumbnail.width() > kMaxDim || thumbnail.height() > kMaxDim) {
            thumbnail = thumbnail.scaled(kMaxDim, kMaxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        emit imageLoaded(thumbnail);
    }

    emit finished();
}
