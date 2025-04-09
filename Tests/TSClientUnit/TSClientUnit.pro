TEMPLATE = app
TARGET = test_tradestationclient
QT += core network testlib widgets webenginewidgets gui
CONFIG += testlib

CONFIG -= gui

include($$PWD/../../Clients/Clients.pri)
include($$PWD/../../Misc/Misc.pri)

#test sources
SOURCES += TestTSClient.cpp main.cpp
HEADERS += TestTSClient.h

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
