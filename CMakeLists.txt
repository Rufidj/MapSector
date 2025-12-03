cmake_minimum_required(VERSION 3.16)

project(MapSector VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Agregar búsqueda de zlib
find_package(ZLIB REQUIRED)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES MapSector_es_ES.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MapSector
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        MapStructures.h
        EditorGraphicsItem.h
        EditorGraphicsItem.cpp
        EditorScene.h
        EditorScene.cpp
        VertexItem.h
        VertexItem.cpp
        WallDialog.h
        WallDialog.cpp
        ZoomableGraphicsView.h
        ZoomableGraphicsView.cpp
        textureselectordialog.h
        textureselectordialog.cpp
        MapStructures.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET MapSector APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(MapSector SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(MapSector
            ${PROJECT_SOURCES}
            MapStructures.h
            EditorGraphicsItem.h
            EditorGraphicsItem.cpp
            EditorScene.h
            EditorScene.cpp
            VertexItem.h
            VertexItem.cpp
            WallDialog.h
            WallDialog.cpp
            ZoomableGraphicsView.h
            ZoomableGraphicsView.cpp
            textureselectordialog.h
            textureselectordialog.cpp
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# IMPORTANTE: Agregar ZLIB::ZLIB aquí para soporte de descompresión gzip
target_link_libraries(MapSector PRIVATE Qt${QT_VERSION_MAJOR}::Widgets ZLIB::ZLIB)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.MapSector)
endif()
set_target_properties(MapSector PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS MapSector
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MapSector)
endif()
