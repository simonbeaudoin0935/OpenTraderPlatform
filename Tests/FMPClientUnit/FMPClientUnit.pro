TEMPLATE = app
TARGET = test_fmpclient
QT += core network testlib
CONFIG += testlib

#test sources
SOURCES += test_fmpclient.cpp main.cpp
HEADERS += test_fmpclient.h

#FMPClient sources
SOURCES += \
    ../../Clients/restclient.cpp \
    ../../Clients/FMPClient/fmpclient.cpp \
    ../../Clients/FMPClient/Filters/companyscreenerfilter.cpp \
    ../../Clients/FMPClient/Filters/stocknewsfilter.cpp

HEADERS += \
    ../../Clients/restclient.h \
    ../../Clients/FMPClient/fmpclient.h \
    ../../Clients/FMPClient/Filters/companyscreenerfilter.h \
    ../../Clients/FMPClient/Filters/stocknewsfilter.h

INCLUDEPATH += ../../Clients/FMPClient
DEPENDPATH += ../../Clients/FMPClient

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
