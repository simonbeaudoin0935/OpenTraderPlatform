TEMPLATE = app
TARGET = test_tradestationclient
QT += core network testlib widgets webenginewidgets gui
CONFIG += testlib

#test sources
SOURCES += TestTradeStationClient.cpp main.cpp
HEADERS += TestTradeStationClient.h

#TradeStationClient sources
SOURCES += \
    ../../Clients/RESTClient.cpp \
    ../../Clients/TradeStationClient/TradeStationClient.cpp \
    ../../Clients/TradeStationClient/OrderExecution/PlaceOrder/PlaceOrder.cpp \
    ../../Clients/TradeStationClient/Brokerage/Accounts/AccountsResult.cpp \
    ../../Clients/TradeStationClient/Auth/AuthWindow.cpp \
    ../../Clients/TradeStationClient/Auth/AuthToken.cpp \
    ../../Clients/TradeStationClient/Auth/ClientToken.cpp

HEADERS += \
    ../../Clients/RESTClient.h \
    ../../Clients/TradeStationClient/TradeStationClient.h \
    ../../Clients/TradeStationClient/OrderExecution/PlaceOrder/PlaceOrder.h \
    ../../Clients/TradeStationClient/Brokerage/Accounts/AccountsResult.h \
    ../../Clients/TradeStationClient/Auth/AuthWindow.h \
    ../../Clients/TradeStationClient/Auth/AuthToken.h \
    ../../Clients/TradeStationClient/Auth/ClientToken.h


INCLUDEPATH += ../../Clients/TradeStationClient
DEPENDPATH += ../../Clients/TradeStationClient

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
