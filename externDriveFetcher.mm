#import "externalDriveFetcher.h"
#include <QDir>

#include <Cocoa/Cocoa.h>

QIcon ExternalDriveIconFetcher::getExternalDriveIcon(const QString &drivePath)
{
  try {
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSString *path = [NSString stringWithUTF8String:drivePath.toStdString().c_str()];
    NSURL *url = [NSURL fileURLWithPath:path];
    NSImage *iconImage = [workspace iconForFile:url.path];

    // Prefer PNG to avoid requiring the Qt TIFF plugin
    NSData *tiffData = [iconImage TIFFRepresentation];
    if (!tiffData) {
      return QIcon();
    }
    NSBitmapImageRep *rep = [NSBitmapImageRep imageRepWithData:tiffData];
    if (!rep) {
      return QIcon();
    }
    NSData *pngData = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    if (!pngData) {
      return QIcon();
    }

    QImage qimg;
    if (!qimg.loadFromData(reinterpret_cast<const uchar *>(pngData.bytes), static_cast<uint>(pngData.length), "PNG")) {
      // Fallback: try raw bitmap data (and copy to own buffer)
      QImage fallbackImg((const uchar *)[rep bitmapData], [rep pixelsWide], [rep pixelsHigh], QImage::Format_RGBA8888);
      if (fallbackImg.isNull()) {
        return QIcon();
      }
      qimg = fallbackImg.copy();
    }

    return QIcon(QPixmap::fromImage(qimg));
  } catch (...) {
    return QIcon();
  }
}
QPixmap ExternalDriveIconFetcher::getExternalDrivePixmap(const QString &drivePath)
{
  try {
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSString *path = [NSString stringWithUTF8String:drivePath.toStdString().c_str()];
    NSURL *url = [NSURL fileURLWithPath:path];
    NSImage *iconImage = [workspace iconForFile:url.path];

    // Prefer PNG to avoid requiring the Qt TIFF plugin
    NSData *tiffData = [iconImage TIFFRepresentation];
    if (!tiffData) {
      return QPixmap();
    }
    NSBitmapImageRep *rep = [NSBitmapImageRep imageRepWithData:tiffData];
    if (!rep) {
      return QPixmap();
    }
    NSData *pngData = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    if (!pngData) {
      return QPixmap();
    }

    QImage qimg;
    if (!qimg.loadFromData(reinterpret_cast<const uchar *>(pngData.bytes), static_cast<uint>(pngData.length), "PNG")) {
      // Fallback: try raw bitmap data (and copy to own buffer)
      QImage fallbackImg((const uchar *)[rep bitmapData], [rep pixelsWide], [rep pixelsHigh], QImage::Format_RGBA8888);
      if (fallbackImg.isNull()) {
        return QPixmap();
      }
      qimg = fallbackImg.copy();
    }

    return QPixmap::fromImage(qimg);
  } catch (...) {
    return QPixmap();
  }
}
