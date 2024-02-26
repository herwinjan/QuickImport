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


ICON = QuickImportLogo-1024.icns

SOURCES += \
    externDriveFetcher.mm \
    filecopydialog.cpp \
    filecopyworker.cpp \
    filelistmodel.cpp \
    qborderlessdialog.cpp \
    selectcarddialog.cpp
LIBS += -framework DiskArbitration -framework Foundation


SOURCES += \
    devicelist.cpp \
    filesmodel.cpp \
    main.cpp \
    mainwindow.cpp


HEADERS += \
    devicelist.h \
    externalDriveFetcher.h \
    filecopydialog.h \
    filecopyworker.h \
    filelistmodel.h \
    filesmodel.h \
    mainwindow.h \
    qborderlessdialog.h \
    hotplugwatcher.h \
    selectcarddialog.h

FORMS += \
    filecopydialog.ui \
    mainwindow.ui \
    selectcarddialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target



win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../../../../opt/homebrew/Cellar/libraw/0.21.2/lib/release/ -lraw.23
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../../../../opt/homebrew/Cellar/libraw/0.21.2/lib/debug/ -lraw.23
else:unix: LIBS += -L$$PWD/../../../../../../opt/homebrew/Cellar/libraw/0.21.2/lib/ -lraw.23

INCLUDEPATH += $$PWD/../../../../../../opt/homebrew/Cellar/libraw/0.21.2/include/
DEPENDPATH += $$PWD/../../../../../../opt/homebrew/Cellar/libraw/0.21.2/include/

DISTFILES += \
    QuickImportLogo-1024.icns

RESOURCES += \
    icons.qrc
