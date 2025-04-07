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
    ../../Clients/TSClient/Brokerage/StreamPositions/Position.cpp \
    ../../Clients/TSClient/Brokerage/StreamPositions/StreamPositions.cpp \
    ../../Clients/TSClient/Brokerage/GetAccounts/Account.cpp \
    ../../Clients/TSClient/Brokerage/GetAccounts/GetAccounts.cpp \
    ../../Clients/TSClient/Stream/Stream.cpp \
    ../../Clients/TSClient/MarketData/GetQuoteSnapshots/QuoteSnapshot.cpp \
    ../../Clients/TSClient/MarketData/MockStream/MockStreamNetworkReply.cpp \
    ../../Clients/TSClient/MarketData/StreamBars/Bar.cpp \
    ../../Clients/TSClient/MarketData/StreamBars/StreamBars.cpp \
    ../../Clients/TSClient/MarketData/StreamMarketDepthQuote/StreamMarketDepthQuote.cpp \
    ../../Clients/TSClient/MarketData/StreamMarketDepthQuote/MarketDepthQuote.cpp \
    ../../Clients/TSClient/Auth/AuthWindow.cpp \
    ../../Clients/TSClient/Auth/AuthToken.cpp \
    ../../Clients/TSClient/Auth/ClientToken.cpp \
    ../../Misc/MarketHours.cpp

HEADERS += \
    ../../Clients/RESTClient.h \
    ../../Clients/TSClient/TSClient.h \
    ../../Clients/TSClient/OrderExecution/PlaceOrder/PlaceOrder.h \
    ../../Clients/TSClient/Brokerage/StreamPositions/Positions.h \
    ../../Clients/TSClient/Brokerage/StreamPositions/StreamPositions.h \
    ../../Clients/TSClient/Brokerage/Accounts/Account.h \
    ../../Clients/TSClient/Brokerage/GetAccounts/GetAccounts.h \
    ../../Clients/TSClient/Stream/Stream.h \
    ../../Clients/TSClient/MarketData/GetQuoteSnapshots/QuoteSnapshot.h \
    ../../Clients/TSClient/MarketData/MockStream/MockStreamNetworkReply.h \
    ../../Clients/TSClient/MarketData/StreamBars/Bar.h \
    ../../Clients/TSClient/MarketData/StreamBars/StreamBars.h \
    ../../Clients/TSClient/MarketData/StreamMarketDepthQuote/StreamMarketDepthQuote.h \
    ../../Clients/TSClient/MarketData/StreamMarketDepthQuote/MarketDepthQuote.h \
    ../../Clients/TSClient/Auth/AuthWindow.h \
    ../../Clients/TSClient/Auth/AuthToken.h \
    ../../Clients/TSClient/Auth/ClientToken.h \
    ../../Misc/MarketHours.h

INCLUDEPATH += ../../Clients/TSClient
DEPENDPATH += ../../Clients/TSClient

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
