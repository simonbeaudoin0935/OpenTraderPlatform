TEMPLATE = app
TARGET = TradingAlgorithm
QT += core network
CONFIG += console c++11

SOURCES += \
    main.cpp \
    ../Algo/mainalgo.cpp \
    ../Core/mainapp.cpp \
    argumentparser.cpp \
    ../Core/appfrontend.cpp \
    ../Core/memorymonitor.cpp \
    ../Clients/FMPClient/fmpclient.cpp \
    ../Clients/FMPClient/companyscreenerfilter.cpp \
    ../Clients/FMPClient/stocknewsfilter.cpp \
    settings.cpp

HEADERS += \
    ../Algo/mainalgo.h \
    ../Core/appfrontend.h \
    ../Core/mainapp.h \
    ../Core/memorymonitor.h \
    ../Clients/FMPClient/fmpclient.h \
    ../Clients/FMPClient/companyscreenerfilter.h \
    ../Clients/FMPClient/stocknewsfilter.h \
    argumentparser.h \
    settings.h

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
    QT += widgets charts    # Adds QtWidgets (and implicitly QtGui)
    DEFINES += GUI_ENABLED  # For conditional compilation in code

    SOURCES += \
        ../GUI/guifrontend.cpp \
        ../GUI/stockpricechart.cpp \

    HEADERS += \
        ../GUI/guifrontend.h \
        ../GUI/stockpricechart.h

    FORMS += \
        ../GUI/guifrontend.ui

}
