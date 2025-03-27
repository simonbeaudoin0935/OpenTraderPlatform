TEMPLATE = app
TARGET = TradingAlgorithm
QT += core network
CONFIG += console c++11

SOURCES += \
    ../Core/mainapp.cpp \
    argumentparser.cpp \
    main.cpp \
    ../Core/appfrontend.cpp \
    ../FMPClient/fmpclient.cpp \
    ../FMPClient/companyscreenerfilter.cpp \
    settings.cpp

HEADERS += \
    ../Core/appfrontend.h \
    ../Core/mainapp.h \
    ../FMPClient/fmpclient.h \
    ../FMPClient/companyscreenerfilter.h \
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
