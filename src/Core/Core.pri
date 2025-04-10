include(../../utils.prf)

SOURCES += $$files($$PWD/*.cpp, true)
HEADERS += $$files($$PWD/*.h, true)

CORE_SUBDIRS = $$findSubdirs($$PWD)
for(dir, CORE_SUBDIRS) {
    INCLUDEPATH += $$dir
    DEPENDPATH += $$dir
    QMAKE_CXXFLAGS += -I$$dir
}

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
QMAKE_CXXFLAGS += -I$$PWD

contains(CONFIG, gui) {
  SOURCES -= $$PWD/TerminalFrontend.cpp
  HEADERS -= $$PWD/TerminalFrontend.h
}
