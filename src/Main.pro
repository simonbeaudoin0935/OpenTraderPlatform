TEMPLATE = app
TARGET = L2Trader
QT += core network sql
CONFIG += console c++17 debug

CONFIG += gui
CONFIG += c++17

# Enable stack trace symbol resolution
QMAKE_LFLAGS += -rdynamic

# QKeychain temporarily disabled due to version mismatch
# DEFINES += QT_KEYCHAIN_LIB
# LIBS += -lqt6keychain

# Version from git
TAG = $$system(git describe --tags --abbrev=0)
HASH = $$system(git rev-parse --short HEAD)
BRANCH = $$system(git rev-parse --abbrev-ref HEAD)
DEFINES += GIT_TAG=\\\"$${TAG}\\\"
DEFINES += GIT_HASH=\\\"$${HASH}\\\"
DEFINES += GIT_BRANCH=\\\"$${BRANCH}\\\"

include($$PWD/Clients/Clients.pri)
include($$PWD/Algo/Algo.pri)
include($$PWD/GUI/GUI.pri)
include($$PWD/Misc/Misc.pri)
include($$PWD/Core/Core.pri)

SOURCES += main.cpp

RESOURCES += ../Resources/Resources.qrc
