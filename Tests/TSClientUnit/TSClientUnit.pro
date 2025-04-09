TEMPLATE = app
TARGET = test_tradestationclient
QT += core network testlib widgets webenginewidgets gui
CONFIG += testlib

CONFIG -= gui

include($$PWD/../../Clients/Clients.pri)

#test sources
SOURCES += TestTSClient.cpp main.cpp
HEADERS += TestTSClient.h

#TradeStationClient sources
SOURCES += \
    ../../Misc/MarketHours.cpp

HEADERS += \
    ../../Misc/MarketHours.h

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
