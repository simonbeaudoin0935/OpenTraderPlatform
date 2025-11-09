TEMPLATE = app
TARGET = test_tradestationclient
QT += core network testlib
CONFIG += testlib

CONFIG -= gui

# QKeychain temporarily disabled due to version mismatch
# DEFINES += QT_KEYCHAIN_LIB
# LIBS += -lqt6keychain

include($$PWD/../../src/Clients/Clients.pri)
include($$PWD/../../src/Misc/Misc.pri)

#test sources
SOURCES += TestTSClient.cpp main.cpp
HEADERS += TestTSClient.h

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
