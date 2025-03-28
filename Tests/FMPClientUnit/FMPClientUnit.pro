TEMPLATE = app
TARGET = test_fmpclient
QT += core network testlib
CONFIG += testlib

#test sources
SOURCES += test_fmpclient.cpp main.cpp
HEADERS += test_fmpclient.h

#FMPClient sources
SOURCES += \
    ../../Clients/FMPClient/fmpclient.cpp \
    ../../Clients/FMPClient/companyscreenerfilter.cpp \
    ../../Clients/FMPClient/stocknewsfilter.cpp

HEADERS += \
    ../../Clients/FMPClient/fmpclient.h \
    ../../Clients/FMPClient/companyscreenerfilter.h \
    ../../Clients/FMPClient/stocknewsfilter.h

INCLUDEPATH += ../../Clients/FMPClient
DEPENDPATH += ../../Clients/FMPClient

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 