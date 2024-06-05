QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

#!include(libQDeviceWatcher.pri): error(could not find ibQDeviceWatcher.pri)
#staticlib|isEqual(STATICLINK, 1): DEFINES += BUILD_QDEVICEWATCHER_STATIC

isEqual(Q_DEVICE_WATCHER_DEBUG, 1) {
    DEFINES += CONFIG_DEBUG
}

QMAKE_MACOSX_DEPLOYMENT_TARGET = 14.0

TRANSLATIONS = quickimport_en.ts quickimport_nl.ts

ICON = QuickImportLogo-1024.icns

SOURCES += \
    aboutdialog.cpp \
    presetdialog.cpp \
    presetlistmodel.cpp \
    qdevicewatcher/qdevicewatcher.cpp \
    filecopydialog.cpp \
    filecopyworker.cpp \
    filelistmodel.cpp \
    imageloader.cpp \
    qborderlessdialog.cpp \
    selectcarddialog.cpp

SOURCES += \
    devicelist.cpp \
    main.cpp \
    mainwindow.cpp


HEADERS += \
    aboutdialog.h \
    devicelist.h \
    externalDriveFetcher.h \
    filecopydialog.h \
    filecopyworker.h \
    filelistmodel.h \
    imageloader.h \
    mainwindow.h \
    presetdialog.h \
    presetlistmodel.h \
    qborderlessdialog.h \
    hotplugwatcher.h \
    qdevicewatcher/qdevicewatcher.h \
    qdevicewatcher/qdevicewatcher_p.h \
    selectcarddialog.h

FORMS += \
    aboutdialog.ui \
    filecopydialog.ui \
    mainwindow.ui \
    presetdialog.ui \
    selectcarddialog.ui

# Default rules for deployment.
#qnx: target.path = /tmp/$${TARGET}/bin
#else: unix:!android: target.path = /opt/$${TARGET}/bin
#!isEmpty(target.path): INSTALLS += target


#win32:CONFIG(release, debug|release): LIBS += -L/opt/homebrew/Cellar/libraw/0.21.2/lib/release/ -lraw.23
#else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/homebrew/Cellar/libraw/0.21.2/lib/debug/ -lraw.23

macx {
    SOURCES += externDriveFetcher.mm
    LIBS += /Users/herwin/devel/LibRaw-0.21.2/lib/.libs/libraw_r.a
    LIBS += -lz
    INCLUDEPATH += /Users/herwin/devel/LibRaw-0.21.2/
    DEPENDPATH += /Users/herwin/devel/LibRaw-0.21.2/

    LIBS += -framework DiskArbitration -framework Foundation
    SOURCES += qdevicewatcher/qdevicewatcher_mac.cpp

}
win32 {
    LIBS += -Lc:\Users\herwin\devel\LibRaw-0.21.2\lib\ -llibraw -Lc:\Users\herwin\devel\LibRaw-0.21.2\bin
    INCLUDEPATH += c:\Users\herwin\devel\LibRaw-0.21.2\
    DEPENDPATH += c:\Users\herwin\devel\LibRaw-0.21.2\

    wince*: SOURCES += qdevicewatcher/qdevicewatcher_wince.cpp
    else:  SOURCES += qdevicewatcher/qdevicewatcher_win32.cpp
    LIBS *= -luser32

}
DISTFILES += \
    QuickImportLogo-1024.icns \
    qdevicewatcher/libQDeviceWatcher.pri \
    quickimport_en.qm \
    quickimport_en.ts \
    quickimport_nl.qm \
    quickimport_nl.ts \
    readme.md

RESOURCES += \
    icons.qrc \
    translations.qrc

SUBDIRS += \
    qdevicewatcher/libQDeviceWatcher.pro
