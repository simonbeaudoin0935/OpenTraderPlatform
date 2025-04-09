TEMPLATE = app
TARGET = test_barcache
QT += core network testlib
CONFIG += testlib

CONFIG -= gui

include($$PWD/../../Clients/Clients.pri)
include($$PWD/../../Misc/Misc.pri)

#test sources
SOURCES += TestBarCache.cpp main.cpp
HEADERS += TestBarCache.h

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
