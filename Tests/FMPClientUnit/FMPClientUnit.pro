TEMPLATE = app
TARGET = test_fmpclient
QT += core network testlib
CONFIG += testlib

CONFIG -= gui

include($$PWD/../../src/Clients/Clients.pri)

#test sources
SOURCES += TestFMPClient.cpp main.cpp
HEADERS += TestFMPClient.h

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
