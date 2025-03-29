TEMPLATE = app
TARGET = test_tradestationclient
QT += core network testlib widgets webenginewidgets gui
CONFIG += testlib

#test sources
SOURCES += test_tradestationclient.cpp main.cpp
HEADERS += test_tradestationclient.h

#TradeStationClient sources
SOURCES += \
    ../../Clients/restclient.cpp \
    ../../Clients/TradeStationClient/tradestationclient.cpp \
    ../../Clients/TradeStationClient/Auth/AuthWindow.cpp

HEADERS += \
    ../../Clients/restclient.h \
    ../../Clients/TradeStationClient/tradestationclient.h \
    ../../Clients/TradeStationClient/Auth/AuthWindow.h


INCLUDEPATH += ../../Clients/TradeStationClient
DEPENDPATH += ../../Clients/TradeStationClient

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
