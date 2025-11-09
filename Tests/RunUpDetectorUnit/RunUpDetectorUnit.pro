TEMPLATE = app
TARGET = test_RunUpDetector
QT += core network sql testlib
CONFIG += testlib

CONFIG -= gui

include($$PWD/../../src/Clients/Clients.pri)
include($$PWD/../../src/Core/Cache/Cache.pri)
include($$PWD/../../src/Algo/RunUpDetector/RunUpDetector.pri)


#test sources
SOURCES += TestRunUpDetector.cpp main.cpp
HEADERS += TestRunUpDetector.h

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
