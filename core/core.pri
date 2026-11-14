INCLUDEPATH += $$PWD/..
msvc:QMAKE_CXXFLAGS += /utf-8
SOURCES += $$PWD/bankreader.cpp $$PWD/library.cpp $$PWD/resultmodel.cpp $$PWD/searchindex.cpp $$PWD/demodata.cpp
HEADERS += $$PWD/bankreader.h $$PWD/library.h $$PWD/resultmodel.h $$PWD/searchindex.h $$PWD/demodata.h

VERSION = $$cat($$PWD/../VERSION.txt)
DEFINES += FXBVIEWER_VERSION=\\\"$$VERSION\\\"
RESOURCES += $$PWD/../assets/resources.qrc
