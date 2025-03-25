TEMPLATE = app
TARGET = trading_algorithm
QT += core network

SOURCES += main.cpp

LIBS += -L../FMPClient -lFMPClient
INCLUDEPATH += ../FMPClient
DEPENDPATH += ../FMPClient
