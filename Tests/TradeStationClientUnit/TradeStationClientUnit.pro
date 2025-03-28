TEMPLATE = app
TARGET = test_tradestationclient
QT += core network testlib
CONFIG += testlib

#test sources
SOURCES += test_tradestationclient.cpp main.cpp
HEADERS += test_tradestationclient.h

#TradeStationClient sources
SOURCES += \
    ../../TradeStationClient/tradestationclient.cpp

HEADERS += \
    ../../TradeStationClient/tradestationclient.h

INCLUDEPATH += ../../TradeStationClient
DEPENDPATH += ../../TradeStationClient

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 