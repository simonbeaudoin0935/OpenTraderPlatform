TEMPLATE = app
TARGET = test_fmpclient
QT += core network testlib
CONFIG += testlib

#test sources
SOURCES += test_fmpclient.cpp main.cpp
HEADERS += test_fmpclient.h

#FMPClient sources
SOURCES += \
    ../../FMPClient/fmpclient.cpp \
    ../../FMPClient/companyscreenerfilter.cpp \
    ../../FMPClient/stocknewsfilter.cpp

HEADERS += \
    ../../FMPClient/fmpclient.h \
    ../../FMPClient/companyscreenerfilter.h \
    ../../FMPClient/stocknewsfilter.h

INCLUDEPATH += ../../FMPClient
DEPENDPATH += ../../FMPClient

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 