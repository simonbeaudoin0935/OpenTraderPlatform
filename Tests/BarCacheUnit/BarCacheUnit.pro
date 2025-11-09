TEMPLATE = app
TARGET = test_barcache
QT += core network sql testlib
CONFIG += testlib

CONFIG -= gui

# QKeychain temporarily disabled due to version mismatch
# DEFINES += QT_KEYCHAIN_LIB
# LIBS += -lqt6keychain

include($$PWD/../../src/Clients/Clients.pri)
include($$PWD/../../src/Core/Cache/Cache.pri)
include($$PWD/../../src/Misc/Misc.pri)

#test sources
SOURCES += TestBarCache.cpp main.cpp
HEADERS += TestBarCache.h

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
