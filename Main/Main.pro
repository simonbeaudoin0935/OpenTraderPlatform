TEMPLATE = app
TARGET = TradingAlgorithm
QT += core network
CONFIG += console c++11

SOURCES += \
    main.cpp \
    settings.cpp \
    argumentparser.cpp \
    ../Algo/mainalgo.cpp \
    ../Core/mainapp.cpp \
    ../Core/appfrontend.cpp \
    ../Core/memorymonitor.cpp \
    ../Clients/restclient.cpp \
    ../Clients/FMPClient/fmpclient.cpp \
    ../Clients/FMPClient/Filters/companyscreenerfilter.cpp \
    ../Clients/FMPClient/Filters/stocknewsfilter.cpp \
    ../Clients/TradeStationClient/tradestationclient.cpp

HEADERS += \
    settings.h \
    argumentparser.h \
    ../Algo/mainalgo.h \
    ../Core/appfrontend.h \
    ../Core/mainapp.h \
    ../Core/memorymonitor.h \
    ../Clients/restclient.h \
    ../Clients/FMPClient/fmpclient.h \
    ../Clients/FMPClient/Filters/companyscreenerfilter.h \
    ../Clients/FMPClient/Filters/stocknewsfilter.h \
    ../Clients/TradeStationClient/tradestationclient.h

INCLUDEPATH += ../Clients/FMPClient
DEPENDPATH += ../Clients/FMPClient

!gui {
    SOURCES += \
        ../Core/terminalfrontend.cpp
    HEADERS += \
        ../Core/terminalfrontend.h
}

# GUI-specific files and module
gui {
    QT += widgets charts webenginewidgets gui  # Adds QtWidgets (and implicitly QtGui)
    DEFINES += GUI_ENABLED  # For conditional compilation in code

    SOURCES += \
        ../GUI/guifrontend.cpp \
        ../GUI/stockpricechart.cpp \
        ../Clients/TradeStationClient/Auth/AuthWindow.cpp

    HEADERS += \
        ../GUI/guifrontend.h \
        ../GUI/stockpricechart.h \
        ../Clients/TradeStationClient/Auth/AuthWindow.h

    FORMS += \
        ../GUI/guifrontend.ui

}
