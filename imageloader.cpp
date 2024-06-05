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

    if (!node->rawProc) {
        del = true;
        rawProc = new LibRaw();

        auto state = rawProc->open_file(m_imagePath.toLatin1().data());
        if (LIBRAW_SUCCESS != state) {
            emit loadingFailed();
            emit finished();
        }

    } else {
        rawProc = node->rawProc;
        del = false;
    }

    QImage thumbnail;

    if (!(rawProc->imgdata.thumbnail.thumb)) {
        rawProc->unpack_thumb();
    }

    if (LIBRAW_THUMBNAIL_JPEG == rawProc->imgdata.thumbnail.tformat) {
        thumbnail.loadFromData((unsigned char *) rawProc->imgdata.thumbnail.thumb,
                               rawProc->imgdata.thumbnail.tlength,
                               "JPEG");
    }
    thumbnail = thumbnail.scaled(1024, 1024, Qt::KeepAspectRatio);

    if (del) {
        rawProc->recycle();
        delete rawProc;
    }

    // Check if the image was loaded successfully
    if (thumbnail.isNull()) {
        emit loadingFailed();
    } else {
        emit imageLoaded(thumbnail);
    }

    emit finished();
}
