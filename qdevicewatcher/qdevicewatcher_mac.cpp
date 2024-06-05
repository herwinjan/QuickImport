/******************************************************************************
  QDeviceWatcherPrivate: watching depends on platform
  Copyright (C) 2014-2015 Wang Bin <wbsecg1@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
******************************************************************************/

#include "qdevicewatcher_p.h"
#include <QtCore/QStringList>

#include <QDebug>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>

static QStringList sDevices; //disk list, or mount point list?

static void onDiskAppear(DADiskRef disk, void *context)
{
    CFDictionaryRef description = DADiskCopyDescription(disk);
    CFURLRef volumePathURL = (CFURLRef) CFDictionaryGetValue(description,
                                                             kDADiskDescriptionVolumePathKey);

    if (volumePathURL == nullptr) {
        //qWarning() << "Cannot retrieve volume path.";
        //   return;
    }
    CFStringRef protocolString = (CFStringRef)
        CFDictionaryGetValue(description, kDADiskDescriptionDeviceProtocolKey);

    CFStringRef mediaContentString = (CFStringRef)
        CFDictionaryGetValue(description, kDADiskDescriptionMediaContentKey);

    QString mediaContent;

    if (mediaContentString != nullptr)
        mediaContent = QString::fromCFString(mediaContentString);

    if (mediaContent == "FDisk_partition_scheme") {
        //qDebug() << "Partionion scheme";
        return;
    }

    /*        
    CFStringRef volumePathString = CFURLCopyFileSystemPath(volumePathURL, kCFURLPOSIXPathStyle);
    
    if (volumePathString != nullptr) {
        // Convert CFStringRef to QString
        QString volumePath = QString::fromCFString(volumePathString);

        // Check if the volume path is not empty
        if (!volumePath.isEmpty()) {
            qDebug() << "Volume path:" << volumePath;
        } else {
            qDebug() << "Volume path is empty.";
            return;
        }

        // Release the CFStringRef when done
        CFRelease(volumePathString);
    } else {
        qWarning() << "Failed to convert CFURLRef to CFStringRef.";
        return;
    }
*/
    if (protocolString != nullptr) {
        QString protocol = QString::fromCFString(protocolString);
        if (protocol != "USB") {
            //  qDebug() << "No USB";
            return;
        }
    }

    QString disk_name = DADiskGetBSDName(disk);
    if (disk_name.count() <= 0) {
        //qDebug() << "No Name";
        return;
    }
    if (sDevices.contains(disk_name))
        return;
    sDevices.append(disk_name);
    QDeviceWatcherPrivate *p = static_cast<QDeviceWatcherPrivate *>(context);
    p->emitDeviceAdded(disk_name);
}

static void onDiskDisappear(DADiskRef disk, void *context)
{
    QString disk_name = DADiskGetBSDName(disk);
    sDevices.removeAll(disk_name); //erase?
    QDeviceWatcherPrivate *p = static_cast<QDeviceWatcherPrivate *>(context);
    p->emitDeviceRemoved(disk_name);
}

QDeviceWatcherPrivate::~QDeviceWatcherPrivate()
{
    stop();
}

bool QDeviceWatcherPrivate::start()
{
    init();
    QThread::start();

    return true;
}

bool QDeviceWatcherPrivate::stop()
{
    mStop = true;
    wait();
    //DAUnregisterApprovalCallback
    DAUnregisterCallback(mSession, (void *) onDiskAppear, this);
    DAUnregisterCallback(mSession, (void *) onDiskDisappear, this);
}

void QDeviceWatcherPrivate::parseDeviceInfo() {}

bool QDeviceWatcherPrivate::init()
{
    //get sDevices
    //FSGetVolumeInfo()
    mSession = DASessionCreate(kCFAllocatorDefault);

    DARegisterDiskAppearedCallback(mSession, NULL, onDiskAppear, this);
    DARegisterDiskDisappearedCallback(mSession, NULL, onDiskDisappear, this);
}

void QDeviceWatcherPrivate::run()
{
    mStop = false;

    DASessionScheduleWithRunLoop(mSession, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    SInt32 result;
    do {
        result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, true);

    } while (!mStop && result);

    DASessionUnscheduleFromRunLoop(mSession, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
}
