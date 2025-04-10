include(../../utils.prf)

SOURCES += $$files($$PWD/*.cpp, true)
HEADERS += $$files($$PWD/*.h, true)

ALGO_SUBDIRS = $$findSubdirs($$PWD)
for(dir, ALGO_SUBDIRS) {
    INCLUDEPATH += $$dir
    DEPENDPATH += $$dir
    QMAKE_CXXFLAGS += -I$$dir
}

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
QMAKE_CXXFLAGS += -I$$PWD
