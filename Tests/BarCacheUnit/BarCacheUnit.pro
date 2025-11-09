TEMPLATE = app
TARGET = test_barcache
QT += core network sql testlib
CONFIG += testlib

CONFIG -= gui

include($$PWD/../../src/Clients/Clients.pri)
include($$PWD/../../src/Core/Cache/Cache.pri)

#test sources
SOURCES += TestBarCache.cpp main.cpp
HEADERS += TestBarCache.h

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
