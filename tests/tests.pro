QT += core gui widgets testlib
CONFIG += c++17 testcase console
TARGET = fxbviewer_tests
TEMPLATE = app
SOURCES += test_fxbviewer.cpp ../mainwindow.cpp
HEADERS += ../mainwindow.h
include(../core/core.pri)
