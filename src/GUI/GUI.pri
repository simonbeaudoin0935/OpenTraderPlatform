
# GUI-specific files and module
contains(CONFIG, gui) {
    QT += widgets charts webenginewidgets gui  # Adds QtWidgets (and implicitly QtGui)
    DEFINES += GUI_ENABLED  # For conditional compilation in code

    SOURCES += $$files($$PWD/*.cpp, true)


    HEADERS += $$files($$PWD/*.h, true)

    FORMS += \
        GUI/GUIFrontend.ui
}
