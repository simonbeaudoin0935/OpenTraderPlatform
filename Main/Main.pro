TEMPLATE = app
TARGET = trading_algorithm
QT += core network

SOURCES += \
    main.cpp \
    ../FMPClient/fmpclient.cpp

HEADERS += ../FMPClient/fmpclient.h

INCLUDEPATH += ../FMPClient
DEPENDPATH += ../FMPClient
