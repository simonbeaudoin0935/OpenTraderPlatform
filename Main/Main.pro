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
    ../FMPClient/fmpclient.cpp \
    ../FMPClient/companyscreenerfilter.cpp \
    ../FMPClient/stocknewsfilter.cpp \
    settings.cpp

HEADERS += \
    ../Algo/mainalgo.h \
    ../Core/appfrontend.h \
    ../Core/mainapp.h \
    ../Core/memorymonitor.h \
    ../FMPClient/fmpclient.h \
    ../FMPClient/companyscreenerfilter.h \
    ../FMPClient/stocknewsfilter.h \
    argumentparser.h \
    settings.h

INCLUDEPATH += ../FMPClient
DEPENDPATH += ../FMPClient

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
