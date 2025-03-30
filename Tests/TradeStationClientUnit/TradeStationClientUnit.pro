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
    ../../Clients/TradeStationClient/Account/accountresult.cpp \
    ../../Clients/TradeStationClient/Auth/AuthWindow.cpp \
    ../../Clients/TradeStationClient/Auth/authtoken.cpp \
    ../../Clients/TradeStationClient/Auth/clienttoken.cpp

HEADERS += \
    ../../Clients/restclient.h \
    ../../Clients/TradeStationClient/tradestationclient.h \
    ../../Clients/TradeStationClient/Account/accountresult.h \
    ../../Clients/TradeStationClient/Auth/AuthWindow.h \
    ../../Clients/TradeStationClient/Auth/authtoken.h \
    ../../Clients/TradeStationClient/Auth/clienttoken.h


INCLUDEPATH += ../../Clients/TradeStationClient
DEPENDPATH += ../../Clients/TradeStationClient

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
