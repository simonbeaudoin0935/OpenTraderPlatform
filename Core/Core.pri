SOURCES += $$files($$PWD/*.cpp, true)
HEADERS += $$files($$PWD/*.h, true)

contains(CONFIG, gui) {
  SOURCES -= $$PWD/TerminalFrontend.cpp
  HEADERS -= $$PWD/TerminalFrontend.h
}
