TEMPLATE = app
TARGET = test_tradestationclient
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
SOURCES += TestTSClient.cpp main.cpp
HEADERS += TestTSClient.h

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 

# Version from git
TAG = $$system(git describe --tags --abbrev=0)
HASH = $$system(git rev-parse --short HEAD)
BRANCH = $$system(git rev-parse --abbrev-ref HEAD)
DEFINES += GIT_TAG=\\\"$${TAG}\\\"
DEFINES += GIT_HASH=\\\"$${HASH}\\\"
DEFINES += GIT_BRANCH=\\\"$${BRANCH}\\\"