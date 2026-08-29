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
    if (description == nullptr)
        return; // disk vanished between callback and query

    QString protocol;
    QString mediaContent;
    bool removable = false;
    bool ejectable = false;

    CFStringRef protocolString = (CFStringRef)
        CFDictionaryGetValue(description, kDADiskDescriptionDeviceProtocolKey);
    if (protocolString != nullptr)
        protocol = QString::fromCFString(protocolString);

    CFStringRef mediaContentString = (CFStringRef)
        CFDictionaryGetValue(description, kDADiskDescriptionMediaContentKey);
    if (mediaContentString != nullptr)
        mediaContent = QString::fromCFString(mediaContentString);

    CFBooleanRef removableRef = (CFBooleanRef)
        CFDictionaryGetValue(description, kDADiskDescriptionMediaRemovableKey);
    if (removableRef != nullptr)
        removable = CFBooleanGetValue(removableRef);

    CFBooleanRef ejectableRef = (CFBooleanRef)
        CFDictionaryGetValue(description, kDADiskDescriptionMediaEjectableKey);
    if (ejectableRef != nullptr)
        ejectable = CFBooleanGetValue(ejectableRef);

    CFRelease(description);

    // Skip whole-disk container objects (FDisk_partition_scheme,
    // GUID_partition_scheme, ...): the interesting event is the partition
    // itself, and reporting both causes duplicate "card inserted" prompts.
    if (mediaContent.endsWith(QLatin1String("_partition_scheme")))
        return;

    // Accept USB/SD readers by protocol, and any removable/ejectable medium
    // (covers CFexpress readers on Thunderbolt/PCIe). Fixed internal disks
    // are neither.
    const bool protocolMatch = protocol == QLatin1String("USB")
                               || protocol.contains(QLatin1String("Secure Digital"));
    if (!protocolMatch && !removable && !ejectable)
        return;

    QString disk_name = DADiskGetBSDName(disk);
    if (disk_name.size() <= 0)
        return;
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
    return true;
}

void QDeviceWatcherPrivate::parseDeviceInfo() {}

bool QDeviceWatcherPrivate::init()
{
    //get sDevices
    //FSGetVolumeInfo()
    mSession = DASessionCreate(kCFAllocatorDefault);

    DARegisterDiskAppearedCallback(mSession, NULL, onDiskAppear, this);
    DARegisterDiskDisappearedCallback(mSession, NULL, onDiskDisappear, this);
    return true;
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
