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

        NSBitmapImageRep *imageRep = [NSBitmapImageRep imageRepWithData:[iconImage TIFFRepresentation]];
        QImage image = QImage([imageRep bitmapData], [imageRep pixelsWide], [imageRep pixelsHigh], QImage::Format_RGBA8888); // Assuming 32-bit RGBA format
        QPixmap pixmap = QPixmap::fromImage(image);
    
    
        return QIcon(pixmap);
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

    // Convert NSImage to QPixmap
    NSBitmapImageRep *imageRep = [NSBitmapImageRep imageRepWithData:[iconImage TIFFRepresentation]];
    QImage image = QImage([imageRep bitmapData], [imageRep pixelsWide], [imageRep pixelsHigh], QImage::Format_RGBA8888); // Assuming 32-bit RGBA format
    QPixmap pixmap = QPixmap::fromImage(image);

    return pixmap;
  } catch (...) {
    return QPixmap();
  }
}
