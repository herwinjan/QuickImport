QT       += core gui concurrent

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
 DEFINES += CONFIG_DEBUG

CONFIG+=app_bundle

QMAKE_MACOSX_DEPLOYMENT_TARGET = 14.0

TRANSLATIONS = quickimport_en.ts quickimport_nl.ts

ICON = QuickImportLogo-1024.icns

SOURCES += \
    aboutdialog.cpp \
    fileinfomodel.cpp \
    metadatadialog.cpp \
    metadatatemplatedialog.cpp \
    presetdialog.cpp \
    presetlistmodel.cpp \
    qdevicewatcher/qdevicewatcher.cpp \
    filecopydialog.cpp \
    filecopyworker.cpp \
    imageloader.cpp \
    qborderlessdialog.cpp \
    selectcarddialog.cpp \
    shortcutdialog.cpp \
    xmpengine.cpp

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
    fileinfomodel.h \
    imageloader.h \
    mainwindow.h \
    metadatadialog.h \
    metadatatemplatedialog.h \
    presetdialog.h \
    presetlistmodel.h \
    qborderlessdialog.h \
    hotplugwatcher.h \
    qdevicewatcher/qdevicewatcher.h \
    qdevicewatcher/qdevicewatcher_p.h \
    selectcarddialog.h \
    shortcutdialog.h \
    xmpengine.h

FORMS += \
    aboutdialog.ui \
    filecopydialog.ui \
    mainwindow.ui \
    metadatadialog.ui \
    metadatatemplatedialog.ui \
    presetdialog.ui \
    selectcarddialog.ui \
    shortcutdialog.ui

# Default rules for deployment.
#qnx: target.path = /tmp/$${TARGET}/bin
#else: unix:!android: target.path = /opt/$${TARGET}/bin
#!isEmpty(target.path): INSTALLS += target


#win32:CONFIG(release, debug|release): LIBS += -L/opt/homebrew/Cellar/libraw/0.21.2/lib/release/ -lraw.23
#else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/homebrew/Cellar/libraw/0.21.2/lib/debug/ -lraw.23

macx {

# export LDFLAGS="-L/opt/homebrew/opt/libxml2/lib"
# export CPPFLAGS="-I/opt/homebrew/opt/libxml2/include"

        # LIBS += -L/opt/homebrew/lib
    SOURCES += externDriveFetcher.mm
    LIBS += /Users/herwin/devel/LibRaw/lib/.libs/libraw_r.a
        LIBS += /Users/herwin/devel/exiv2/lib/libexiv2.a
        LIBS += -lz         -lexpat -liconv
        #-liconv -lINIReader
    INCLUDEPATH += /Users/herwin/devel/LibRaw/
    INCLUDEPATH += /Users/herwin/devel/exiv2/include
    INCLUDEPATH += /Users/herwin/devel/exiv2
    DEPENDPATH += /Users/herwin/devel/LibRaw/

    LIBS += -framework DiskArbitration -framework Foundation
    SOURCES += qdevicewatcher/qdevicewatcher_mac.cpp

}
win32 {
    LIBS += -Lc:\Users\herwin\devel\LibRaw\lib\ -llibraw -Lc:\Users\herwin\devel\LibRaw\bin
    INCLUDEPATH += c:\Users\herwin\devel\LibRaw\
    DEPENDPATH += c:\Users\herwin\devel\LibRaw\

    wince*: SOURCES += qdevicewatcher/qdevicewatcher_wince.cpp
    else:  SOURCES += qdevicewatcher/qdevicewatcher_win32.cpp
    LIBS *= -luser32

}
DISTFILES += \
    QuickImportLogo-1024.icns \
    images/QuickImport-1.png \
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
