TEMPLATE = app
TARGET = Recorder
QT += core network sql
CONFIG += console c++17 debug

# Enable stack trace symbol resolution
QMAKE_LFLAGS += -rdynamic

# QKeychain temporarily disabled due to version mismatch
# DEFINES += QT_KEYCHAIN_LIB
# LIBS += -lqt6keychain

include($$PWD/Clients/Clients.pri)
include($$PWD/Algo/Algo.pri)
include($$PWD/Misc/Misc.pri)
include($$PWD/Core/Core.pri)

SOURCES += recorder_main.cpp

RESOURCES += ../Resources/Resources.qrc