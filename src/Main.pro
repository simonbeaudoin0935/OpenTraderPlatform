TEMPLATE = app
TARGET = L2Trader
QT += core network sql
CONFIG += console c++11

CONFIG += gui
CONFIG += c++17

# QKeychain temporarily disabled due to version mismatch
# DEFINES += QT_KEYCHAIN_LIB
# LIBS += -lqt6keychain

include($$PWD/Clients/Clients.pri)
include($$PWD/Algo/Algo.pri)
include($$PWD/GUI/GUI.pri)
include($$PWD/Misc/Misc.pri)
include($$PWD/Core/Core.pri)

SOURCES += main.cpp

RESOURCES += ../Resources/Resources.qrc
