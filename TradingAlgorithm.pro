TEMPLATE = subdirs
SUBDIRS += \
    FMPClient \
    Tests \
    Main

Tests.depends = FMPClient
Main.depends = FMPClient
