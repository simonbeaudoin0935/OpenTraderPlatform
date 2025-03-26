TEMPLATE = app
TARGET = test_fmpclient
QT += core network testlib
CONFIG += testlib

#test sources
SOURCES += FMPClientUnit/test_fmpclient.cpp FMPClientUnit/main.cpp
HEADERS += FMPClientUnit/test_fmpclient.h

#FMPClient sources
SOURCES += ../FMPClient/fmpclient.cpp
HEADERS += ../FMPClient/fmpclient.h
INCLUDEPATH += ../FMPClient
DEPENDPATH += ../FMPClient

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING
