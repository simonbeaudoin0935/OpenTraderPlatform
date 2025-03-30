TEMPLATE = app
TARGET = test_fmpclient
QT += core network testlib
CONFIG += testlib

#test sources
SOURCES += TestFMPClient.cpp main.cpp
HEADERS += TestFMPClient.h

#FMPClient sources
SOURCES += \
    ../../Clients/RESTClient.cpp \
    ../../Clients/FMPClient/FMPClient.cpp \
    ../../Clients/FMPClient/Filters/CompanyScreenerFilter.cpp \
    ../../Clients/FMPClient/Filters/StockNewsFilter.cpp

HEADERS += \
    ../../Clients/RESTClient.h \
    ../../Clients/FMPClient/FMPClient.h \
    ../../Clients/FMPClient/Filters/CompanyScreenerF`ilter.h \
    ../../Clients/FMPClient/Filters/StockNewsFilter.h

INCLUDEPATH += ../../Clients/FMPClient
DEPENDPATH += ../../Clients/FMPClient

INCLUDEPATH

QMAKE_CXXFLAGS += -Og

DEFINES += UNIT_TESTING 
