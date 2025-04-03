TEMPLATE = app
TARGET = TradingAlgorithm
QT += core network
CONFIG += console c++11

SOURCES += \
    main.cpp \
    Misc/Settings.cpp \
    Misc/ArgumentParser.cpp \
    Misc/MarketHours.cpp \
    Algo/MainAlgo.cpp \
    Algo/BreakingNewsFetcher/BreakingNewsFetcher.cpp \
    Algo/MarketDepthQuoteReceiver/MarketDepthQuoteReceiver.cpp \
    Algo/StockBarsReceiver/StockBarsReceiver.cpp \
    Algo/StockScreener/StockScreener.cpp \
    Core/MainApp.cpp \
    Core/AppFrontend.cpp \
    Core/MemoryMonitor.cpp \
    Clients/RESTClient.cpp \
    Clients/FMPClient/FMPClient.cpp \
    Clients/FMPClient/Filters/CompanyScreenerFilter.cpp \
    Clients/FMPClient/Filters/StockNewsFilter.cpp \
    Clients/TSClient/TSClient.cpp \
    Clients/TSClient/Auth/AuthToken.cpp \
    Clients/TSClient/Auth/ClientToken.cpp \
    Clients/TSClient/Stream/Stream.cpp \
    Clients/TSClient/MarketData/MockStream/MockStreamNetworkReply.cpp \
    Clients/TSClient/MarketData/StreamBars/Bar.cpp \
    Clients/TSClient/MarketData/StreamBars/StreamBars.cpp \
    Clients/TSClient/MarketData/StreamMarketDepthQuote/StreamMarketDepthQuote.cpp \
    Clients/TSClient/MarketData/StreamMarketDepthQuote/MarketDepthQuote.cpp \
    Clients/TSClient/Brokerage/GetAccounts/Account.cpp \
    Clients/TSClient/Brokerage/GetAccounts/GetAccounts.cpp \
    Clients/TSClient/Brokerage/StreamPositions/Position.cpp \
    Clients/TSClient/Brokerage/StreamPositions/StreamPositions.cpp \
    Clients/TSClient/OrderExecution/PlaceOrder/PlaceOrder.cpp

HEADERS += \
    Misc/Settings.h \
    Misc/ArgumentParser.h \
    Misc/MarketHours.h \
    Algo/MainAlgo.h \
    Algo/BreakingNewsFetcher/BreakingNewsFetcher.h \
    Algo/StockBarsReceiver/StockBarsReceiver.h \
    Algo/MarketDepthQuoteReceiver/MarketDepthQuoteReceiver.h \
    Algo/StockScreener/StockScreener.h \
    Core/AppFrontend.h \
    Core/MainApp.h \
    Core/MemoryMonitor.h \
    Clients/RESTClient.h \
    Clients/FMPClient/FMPClient.h \
    Clients/FMPClient/Filters/CompanyScreenerFilter.h \
    Clients/FMPClient/Filters/StockNewsFilter.h \
    Clients/TSClient/TSClient.h \
    Clients/TSClient/Auth/AuthToken.h \
    Clients/TSClient/Auth/ClientToken.h \
    Clients/TSClient/Stream/Stream.h \
    Clients/TSClient/MarketData/MockStream/MockStreamNetworkReply.h \
    Clients/TSClient/MarketData/StreamBars/Bar.h \
    Clients/TSClient/MarketData/StreamBars/StreamBars.h \
    Clients/TSClient/MarketData/StreamMarketDepthQuote/StreamMarketDepthQuote.h \
    Clients/TSClient/MarketData/StreamMarketDepthQuote/MarketDepthQuote.h \
    Clients/TSClient/Brokerage/GetAccounts/Account.h \
    Clients/TSClient/Brokerage/GetAccounts/GetAccounts.h \
    Clients/TSClient/Brokerage/StreamPositions/Positions.h \
    Clients/TSClient/Brokerage/StreamPositions/StreamPositions.h \
    Clients/TSClient/OrderExecution/PlaceOrder/PlaceOrder.h

INCLUDEPATH += \
    Clients/FMPClient
    Clients/TSClient

DEPENDPATH += Clients/FMPClient

!gui {
    SOURCES += \
        Core/TerminalFrontend.cpp
    HEADERS += \
        Core/TerminalFrontend.h
}

# GUI-specific files and module
gui {
    QT += widgets charts webenginewidgets gui  # Adds QtWidgets (and implicitly QtGui)
    DEFINES += GUI_ENABLED  # For conditional compilation in code

    SOURCES += \
        GUI/GuiFrontend.cpp \
        GUI/StockPriceChart.cpp \
        GUI/MarketDepthTableView.cpp \
        GUI/MarketDepthTable.cpp \
        Clients/TSClient/Auth/AuthWindow.cpp

    HEADERS += \
        GUI/GuiFrontend.h \
        GUI/StockPriceChart.h \
        GUI/MarketDepthTableView.h \
        GUI/MarketDepthTable.h \
        Clients/TSClient/Auth/AuthWindow.h \

    FORMS += \
        GUI/guifrontend.ui
}
