TEMPLATE = app
TARGET = test_tradestationclient
QT += core network testlib widgets webenginewidgets gui
CONFIG += testlib

#test sources
SOURCES += TestTSClient.cpp main.cpp
HEADERS += TestTSClient.h

#TradeStationClient sources
SOURCES += \
    ../../Clients/RESTClient.cpp \
    ../../Clients/TSClient/TSClient.cpp \
    ../../Clients/TSClient/OrderExecution/PlaceOrder/PlaceOrder.cpp \
    ../../Clients/TSClient/Brokerage/Accounts/AccountsResult.cpp \
    ../../Clients/TSClient/MarketData/Stream.cpp \
    ../../Clients/TSClient/MarketData/StreamMarketDepthQuote/StreamMarketDepthQuote.cpp \
    ../../Clients/TSClient/MarketData/StreamMarketDepthQuote/MarketDepthQuote.cpp \
    ../../Clients/TSClient/Auth/AuthWindow.cpp \
    ../../Clients/TSClient/Auth/AuthToken.cpp \
    ../../Clients/TSClient/Auth/ClientToken.cpp

HEADERS += \
    ../../Clients/RESTClient.h \
    ../../Clients/TSClient/TSClient.h \
    ../../Clients/TSClient/OrderExecution/PlaceOrder/PlaceOrder.h \
    ../../Clients/TSClient/Brokerage/Accounts/AccountsResult.h \
    ../../Clients/TSClient/MarketData/Stream.h \
    ../../Clients/TSClient/MarketData/StreamMarketDepthQuote/StreamMarketDepthQuote.h \
    ../../Clients/TSClient/MarketData/StreamMarketDepthQuote/MarketDepthQuote.h \
    ../../Clients/TSClient/Auth/AuthWindow.h \
    ../../Clients/TSClient/Auth/AuthToken.h \
    ../../Clients/TSClient/Auth/ClientToken.h


INCLUDEPATH += ../../Clients/TSClient
DEPENDPATH += ../../Clients/TSClient

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
