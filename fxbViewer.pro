QT += core gui widgets
CONFIG += c++17
TARGET = fxbViewer
TEMPLATE = app
SOURCES += main.cpp mainwindow.cpp
HEADERS += mainwindow.h
include(core/core.pri)

win32 {
    QMAKE_TARGET_PRODUCT = FXB Viewer
    QMAKE_TARGET_DESCRIPTION = FXB and FXP preset search
    QMAKE_TARGET_COMPANY = Petar Juric
    QMAKE_TARGET_COPYRIGHT = Copyright Petar Juric. All rights reserved.
    CONFIG += force_debug_info
}
