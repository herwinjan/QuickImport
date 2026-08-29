#include "imageloader.h"

#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMutexLocker>
#include <libraw/libraw.h>
#include <memory>

namespace {
constexpr int kMaxDim = 1024;

QImage makePlaceholder()
{
    QImage img(kMaxDim, (kMaxDim * 2) / 3, QImage::Format_RGB32);
    img.fill(Qt::black);
    return img;
}
}

imageLoader::imageLoader(QObject *parent)
    : QObject(parent)
{
    // ~100 MB of decoded previews (cost unit is KB); a 1024px RGB32 image
    // is ~4 MB, so roughly the last 25 images stay instant.
    m_cache.setMaxCost(100 * 1024);
}

void imageLoader::requestImage(const QString &path)
{
    QMutexLocker lock(&m_mutex);
    m_pending = path;
    scheduleWake();
}

void imageLoader::requestPrefetch(const QStringList &paths)
{
    QMutexLocker lock(&m_mutex);
    m_prefetch = paths;
    scheduleWake();
}

void imageLoader::clearCache()
{
    m_cache.clear();
}

void imageLoader::scheduleWake()
{
    if (m_wakeQueued)
        return;
    m_wakeQueued = true;
    QMetaObject::invokeMethod(this, &imageLoader::processRequests, Qt::QueuedConnection);
}

void imageLoader::processRequests()
{
    for (;;) {
        QString path;
        bool prefetchOnly = false;
        {
            QMutexLocker lock(&m_mutex);
            if (!m_pending.isEmpty()) {
                path = m_pending;
                m_pending.clear();
            } else if (!m_prefetch.isEmpty()) {
                path = m_prefetch.takeFirst();
                prefetchOnly = true;
            } else {
                m_wakeQueued = false;
                return;
            }
        }

        if (QImage *cached = m_cache.object(path)) {
            if (!prefetchOnly)
                emit imageLoaded(path, *cached, false);
            continue;
        }

        bool failed = false;
        const QImage img = loadFromDisk(path, &failed);
        if (!failed) {
            const qsizetype costKb = qMax<qsizetype>(1, img.sizeInBytes() / 1024);
            m_cache.insert(path, new QImage(img), costKb);
        }
        if (!prefetchOnly)
            emit imageLoaded(path, img, failed);
    }
}

QImage imageLoader::loadFromDisk(const QString &path, bool *failed)
{
    *failed = false;
    QImage thumbnail;

    const QString suffix = QFileInfo(path).suffix().toLower();
    const QList<QByteArray> supported = QImageReader::supportedImageFormats();

    if (supported.contains(suffix.toLocal8Bit())) {
        QImageReader reader(path);
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
            QImage img(path);
            if (!img.isNull())
                thumbnail = img;
        }
    } else {
        std::unique_ptr<LibRaw> rawProc(new LibRaw());
        const QByteArray encodedPath = QFile::encodeName(path);
        if (LIBRAW_SUCCESS == rawProc->open_file(encodedPath.constData())) {
            int ret = LIBRAW_SUCCESS;
            if (!rawProc->imgdata.thumbnail.thumb)
                ret = rawProc->unpack_thumb();

            if (ret == LIBRAW_SUCCESS && rawProc->imgdata.thumbnail.thumb
                && rawProc->imgdata.thumbnail.tlength > 0) {
                QByteArray thumbData(reinterpret_cast<const char *>(
                                         rawProc->imgdata.thumbnail.thumb),
                                     static_cast<int>(rawProc->imgdata.thumbnail.tlength));
                QBuffer buf(&thumbData);
                buf.open(QIODevice::ReadOnly);
                QImageReader r(&buf);
                r.setDecideFormatFromContent(true);
                r.setAutoTransform(true);
                if (!r.read(&thumbnail)) {
                    QImage tmp;
                    if (tmp.loadFromData(reinterpret_cast<const uchar *>(thumbData.constData()),
                                         thumbData.size()))
                        thumbnail = tmp;
                }
            }
        }
    }

    if (thumbnail.isNull()) {
        *failed = true;
        return makePlaceholder();
    }

    // Scale once, and only when actually larger than the preview size
    if (thumbnail.width() > kMaxDim || thumbnail.height() > kMaxDim)
        thumbnail = thumbnail.scaled(kMaxDim, kMaxDim, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
    return thumbnail;
}
