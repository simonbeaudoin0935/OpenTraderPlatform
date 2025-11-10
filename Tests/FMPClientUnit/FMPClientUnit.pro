TEMPLATE = app
TARGET = test_fmpclient
QT += core network testlib
CONFIG += testlib

CONFIG -= gui

# Enable stack trace symbol resolution
QMAKE_LFLAGS += -rdynamic

# QKeychain temporarily disabled due to version mismatch
# DEFINES += QT_KEYCHAIN_LIB
# LIBS += -lqt6keychain

include($$PWD/../../src/Clients/Clients.pri)
include($$PWD/../../src/Misc/Misc.pri)

#test sources
SOURCES += TestFMPClient.cpp main.cpp
HEADERS += TestFMPClient.h

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
