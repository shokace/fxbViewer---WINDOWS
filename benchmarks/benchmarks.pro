QT += core gui widgets
CONFIG += c++17 console
TEMPLATE = app
TARGET = fxbviewer_benchmark
SOURCES += benchmark.cpp
HEADERS += ../core/demodata.h
include(../core/core.pri)
