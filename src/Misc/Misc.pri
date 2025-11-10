include(../common.pri)

SOURCES += $$files($$PWD/*.cpp, true)
HEADERS += $$files($$PWD/*.h, true)

# Generate logging categories
QMAKE_EXTRA_TARGETS += generate_logging
generate_logging.target = $$PWD/Logging_generated.h
generate_logging.commands = python3 $$PWD/generate_logging_categories.py > $$PWD/Logging_generated.h
generate_logging.depends = $$files($$PWD/../*.cpp, true)
PRE_TARGETDEPS += $$PWD/Logging_generated.h

MISC_SUBDIRS = $$findSubdirs($$PWD)
for(dir, MISC_SUBDIRS) {
    INCLUDEPATH += $$dir
    DEPENDPATH += $$dir
    QMAKE_CXXFLAGS += -I$$dir
}

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
QMAKE_CXXFLAGS += -I$$PWD

