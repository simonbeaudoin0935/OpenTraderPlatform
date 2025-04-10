include(../common.pri)

SOURCES += $$files($$PWD/*.cpp, true)
HEADERS += $$files($$PWD/*.h, true)

!contains(CONFIG, gui) {
  SOURCES -= $$PWD/TSClient/Auth/AuthWindow.cpp
  HEADERS -= $$PWD/TSClient/Auth/AuthWindow.h
}

CLIENT_SUBDIRS = $$findSubdirs($$PWD)
for(dir, CLIENT_SUBDIRS) {
    INCLUDEPATH += $$dir
    DEPENDPATH += $$dir
    QMAKE_CXXFLAGS += -I$$dir
}

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
QMAKE_CXXFLAGS += -I$$PWD
