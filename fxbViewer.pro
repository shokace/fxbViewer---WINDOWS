QT += core gui widgets
CONFIG += c++17
TARGET = fxbViewer
TEMPLATE = app
SOURCES += main.cpp mainwindow.cpp
HEADERS += mainwindow.h
include(core/core.pri)
