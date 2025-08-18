cmake_minimum_required(VERSION 3.21)
project(QuickImport LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Qt automoc/uic/rcc ----
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---- C++ standard ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Qt 6 ----
find_package(Qt6 REQUIRED COMPONENTS Core Gui Concurrent Widgets LinguistTools)

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
)

target_compile_definitions(QuickImport PRIVATE
    CONFIG_DEBUG
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
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
    target_include_directories(QuickImport PRIVATE
        /Users/herwin/devel/LibRaw/
        /Users/herwin/devel/exiv2/include
        /Users/herwin/devel/exiv2
    )

    # Static libs & system deps
    target_link_libraries(QuickImport PRIVATE
        /Users/herwin/devel/LibRaw/lib/.libs/libraw_r.a
        /Users/herwin/devel/exiv2/lib/libexiv2.a
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

    # Include & link dirs for LibRaw
    target_include_directories(QuickImport PRIVATE
        c:/Users/herwin/devel/LibRaw/
    )
    link_directories(
        c:/Users/herwin/devel/LibRaw/lib/
        c:/Users/herwin/devel/LibRaw/bin
    )

    target_link_libraries(QuickImport PRIVATE
        libraw
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