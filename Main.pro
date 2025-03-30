TEMPLATE = app
TARGET = TradingAlgorithm
QT += core network
CONFIG += console c++11

SOURCES += \
    main.cpp \
    Misc/Settings.cpp \
    Misc/ArgumentParser.cpp \
    Algo/MainAlgo.cpp \
    Core/MainApp.cpp \
    Core/AppFrontend.cpp \
    Core/MemoryMonitor.cpp \
    Clients/RESTClient.cpp \
    Clients/FMPClient/FMPClient.cpp \
    Clients/FMPClient/Filters/CompanyScreenerFilter.cpp \
    Clients/FMPClient/Filters/StockNewsFilter.cpp \
    Clients/TradeStationClient/TradeStationClient.cpp \
    Clients/TradeStationClient/Auth/AuthToken.cpp \
    Clients/TradeStationClient/Auth/ClientToken.cpp \
    Clients/TradeStationClient/Account/AccountResult.cpp \
    Clients/TradeStationClient/OrderExecution/PlaceOrder/PlaceOrder.cpp

HEADERS += \
    Misc/Settings.h \
    Misc/ArgumentParser.h \
    Algo/MainAlgo.h \
    Core/AppFrontend.h \
    Core/MainApp.h \
    Core/MemoryMonitor.h \
    Clients/RESTClient.h \
    Clients/FMPClient/FMPClient.h \
    Clients/FMPClient/Filters/CompanyScreenerFilter.h \
    Clients/FMPClient/Filters/StockNewsFilter.h \
    Clients/TradeStationClient/TradeStationClient.h \
    Clients/TradeStationClient/Auth/AuthToken.h \
    Clients/TradeStationClient/Auth/ClientToken.h \
    Clients/TradeStationClient/Account/AccountResult.h \
    Clients/TradeStationClient/OrderExecution/PlaceOrder/PlaceOrder.h

INCLUDEPATH += \
    Clients/FMPClient
    Clients/TradeStationClient

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
        Clients/TradeStationClient/Auth/AuthWindow.cpp

    HEADERS += \
        GUI/GuiFrontend.h \
        GUI/StockPriceChart.h \
        Clients/TradeStationClient/Auth/AuthWindow.h \

    FORMS += \
        GUI/guifrontend.ui
}
