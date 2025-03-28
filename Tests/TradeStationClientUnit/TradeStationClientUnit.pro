TEMPLATE = app
TARGET = test_tradestationclient
QT += core network testlib
CONFIG += testlib

#test sources
SOURCES += test_tradestationclient.cpp main.cpp
HEADERS += test_tradestationclient.h

#TradeStationClient sources
SOURCES += \
    ../../Clients/TradeStationClient/tradestationclient.cpp

HEADERS += \
    ../../Clients/TradeStationClient/tradestationclient.h

INCLUDEPATH += ../../Clients/TradeStationClient
DEPENDPATH += ../../Clients/TradeStationClient

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 