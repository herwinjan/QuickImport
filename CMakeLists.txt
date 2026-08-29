cmake_minimum_required(VERSION 3.21)
project(QuickImport VERSION 0.7.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Qt automoc/uic/rcc ----
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---- C++ standard ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- macOS OpenGL compatibility ----
# Qt's FindWrapOpenGL tries to link the deprecated AGL framework on macOS.
# Recent Xcode/CLT setups no longer ship a linkable AGL framework, so provide
# a compatible replacement target before Qt is discovered.
if(APPLE AND NOT TARGET WrapOpenGL::WrapOpenGL)
    add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
    target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE "-framework OpenGL")
    set(WrapOpenGL_FOUND ON)
endif()

# ---- Qt 6 ----
find_package(Qt6 REQUIRED COMPONENTS Core Gui Concurrent Widgets LinguistTools)

set(LIBRAW_ROOT "" CACHE PATH "Optional root path for a LibRaw installation")
set(EXIV2_ROOT "" CACHE PATH "Optional root path for an Exiv2 installation")

# Local source builds go BEFORE the system prefixes: Homebrew builds its
# libraries for the host macOS only, which conflicts with our 14.0
# deployment target (linker warning "built for newer version").
set(QUICKIMPORT_DEP_HINTS
    ${LIBRAW_ROOT}
    ${EXIV2_ROOT}
    $ENV{LIBRAW_ROOT}
    $ENV{EXIV2_ROOT}
    /Users/herwin/devel/LibRaw
    /Users/herwin/devel/exiv2
    /opt/homebrew
    /usr/local
)

find_path(LIBRAW_INCLUDE_DIR
    NAMES libraw/libraw.h
    HINTS ${QUICKIMPORT_DEP_HINTS}
    PATH_SUFFIXES include
)

find_library(LIBRAW_LIBRARY
    NAMES raw_r libraw_r raw libraw
    HINTS ${QUICKIMPORT_DEP_HINTS}
    PATH_SUFFIXES lib lib64 lib/.libs bin
)

find_path(EXIV2_INCLUDE_DIR
    NAMES exiv2/exiv2.hpp
    HINTS ${QUICKIMPORT_DEP_HINTS}
    PATH_SUFFIXES include
)

find_library(EXIV2_LIBRARY
    NAMES exiv2 libexiv2
    HINTS ${QUICKIMPORT_DEP_HINTS}
    PATH_SUFFIXES lib lib64
)

# exiv2lib_export.h is generated into the exiv2 build tree; when building
# against an exiv2 source/build tree it lives outside the include/ directory.
find_path(EXIV2_EXPORT_INCLUDE_DIR
    NAMES exiv2lib_export.h
    HINTS ${QUICKIMPORT_DEP_HINTS} ${EXIV2_INCLUDE_DIR}
    PATH_SUFFIXES . include include/exiv2
)

if(NOT LIBRAW_INCLUDE_DIR OR NOT LIBRAW_LIBRARY)
    message(FATAL_ERROR "LibRaw not found. Set LIBRAW_ROOT or install libraw.")
endif()

if(NOT EXIV2_INCLUDE_DIR OR NOT EXIV2_LIBRARY)
    message(FATAL_ERROR "Exiv2 not found. Set EXIV2_ROOT or install exiv2.")
endif()


set(TS_FILES
    quickimport_en.ts
    quickimport_nl.ts
    
)

# ---- Sources & headers ----
set(PROJECT_SOURCES
    # .cpp
    aboutdialog.cpp
    fileinfomodel.cpp
    metadatadialog.cpp
    metadatatemplatedialog.cpp
    presetdialog.cpp
    presetlistmodel.cpp
    qdevicewatcher/qdevicewatcher.cpp
    filecopydialog.cpp
    filecopyworker.cpp
    imageloader.cpp
    qborderlessdialog.cpp
    selectcarddialog.cpp
    shortcutdialog.cpp
    xmpengine.cpp
    devicelist.cpp
    main.cpp
    mainwindow.cpp

    # .h
    aboutdialog.h
    devicelist.h
    externalDriveFetcher.h
    filecopydialog.h
    filecopyworker.h
    fileinfomodel.h
    imageloader.h
    mainwindow.h
    metadatadialog.h
    metadatatemplatedialog.h
    presetdialog.h
    presetlistmodel.h
    qborderlessdialog.h
    hotplugwatcher.h
    qdevicewatcher/qdevicewatcher.h
    qdevicewatcher/qdevicewatcher_p.h
    selectcarddialog.h
    shortcutdialog.h
    xmpengine.h

    # .ui
    aboutdialog.ui
    filecopydialog.ui
    mainwindow.ui
    metadatadialog.ui
    metadatatemplatedialog.ui
    presetdialog.ui
    selectcarddialog.ui
    shortcutdialog.ui

    # .qrc
    icons.qrc

)

# ---- Executable (Qt6 auto-handles moc/uic/rcc) ----
qt_add_executable(QuickImport
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

# Make project headers visible to AUTOUIC/AUTOMOC generated files
target_include_directories(QuickImport
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/qdevicewatcher
        ${CMAKE_CURRENT_BINARY_DIR} # (good practice for generated headers)
        ${LIBRAW_INCLUDE_DIR}
        ${EXIV2_INCLUDE_DIR}
)

if(EXIV2_EXPORT_INCLUDE_DIR)
    target_include_directories(QuickImport PRIVATE ${EXIV2_EXPORT_INCLUDE_DIR})
endif()

target_compile_definitions(QuickImport PRIVATE
    CONFIG_DEBUG
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
    QUICKIMPORT_VERSION="${PROJECT_VERSION}"
)


target_link_libraries(QuickImport PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
    Qt6::Widgets
)

# ---- Translations (.ts -> .qm -> embedded at :/translation) ----
# Build .qm files with lrelease explicitly, then embed them via rcc
# TS files are listed in TS_FILES above and live in the source root

# Output directory for compiled .qm files
set(QM_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/translations)
file(MAKE_DIRECTORY ${QM_OUTPUT_DIR})

# Map each .ts to a .qm in the build tree
set(QM_FILES)
foreach(_ts ${TS_FILES})
    get_filename_component(_ts_name ${_ts} NAME_WE)
    set(_qm ${QM_OUTPUT_DIR}/${_ts_name}.qm)
    list(APPEND QM_FILES ${_qm})

    add_custom_command(
        OUTPUT ${_qm}
        COMMAND $<TARGET_FILE:Qt6::lrelease> ${CMAKE_CURRENT_SOURCE_DIR}/${_ts} -qm ${_qm}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_ts}
        VERBATIM
        COMMENT "lrelease ${_ts} -> ${_qm}"
    )
endforeach()

# Aggregate target to build all .qm files
add_custom_target(QuickImport_ts ALL DEPENDS ${QM_FILES})

# Generate a QRC that lists the compiled .qm files and embed it via AUTORCC
set(GENERATED_QM_QRC ${CMAKE_CURRENT_BINARY_DIR}/translations.qrc)
file(WRITE ${GENERATED_QM_QRC} "<RCC>\n  <qresource prefix=\"/translation\">\n")
foreach(_qm ${QM_FILES})
    get_filename_component(_qm_name ${_qm} NAME)
    file(APPEND ${GENERATED_QM_QRC} "    <file alias=\"${_qm_name}\">${_qm}</file>\n")
endforeach()
file(APPEND ${GENERATED_QM_QRC} "  </qresource>\n</RCC>\n")

# Ensure the app builds .qm before rcc processes the qrc
add_dependencies(QuickImport QuickImport_ts)

# AUTORCC will pick up this qrc and embed the translations at :/translation
target_sources(QuickImport PRIVATE ${GENERATED_QM_QRC})

# ---- App bundle / icon ----
if(APPLE)
    set_target_properties(QuickImport PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE QuickImportLogo-1024.icns
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
    # Place the .icns into the app bundle Resources
    set_source_files_properties(QuickImportLogo-1024.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(QuickImport PRIVATE QuickImportLogo-1024.icns)

    # macOS deployment target (equivalent of QMAKE_MACOSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "" FORCE)

    # Objective-C++ & platform sources
    target_sources(QuickImport PRIVATE
        externDriveFetcher.mm
        qdevicewatcher/qdevicewatcher_mac.cpp
    )

    # Include paths
    # System deps
    target_link_libraries(QuickImport PRIVATE
        ${LIBRAW_LIBRARY}
        ${EXIV2_LIBRARY}
        z
        expat
        iconv
        "-framework DiskArbitration"
        "-framework Foundation"
    )

endif()

# ---- Windows specifics ----
if(WIN32)
    target_sources(QuickImport PRIVATE
        qdevicewatcher/qdevicewatcher_win32.cpp
    )

    target_link_libraries(QuickImport PRIVATE
        ${LIBRAW_LIBRARY}
        ${EXIV2_LIBRARY}
        user32
    )
endif()

# ---- Extra assets (not strictly required by CMake, but kept for parity) ----
# These are tracked files you ship with the project (e.g., for packaging)
set(DISTFILES
    QuickImportLogo-1024.icns
    images/QuickImport-1.png
    qdevicewatcher/libQDeviceWatcher.pri
    readme.md
)



# You can attach them to the target if you want IDE visibility:
# target_sources(QuickImport PRIVATE ${DISTFILES})

# Note: Your .pro added a qmake subproject: qdevicewatcher/libQDeviceWatcher.pro.
# In CMake we already compile its sources directly. If you later convert that
# subproject to CMake, you can add_subdirectory(qdevicewatcher) and link it instead.



qt_finalize_executable(QuickImport)
