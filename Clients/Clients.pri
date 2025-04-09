SOURCES += $$files($$PWD/*.cpp, true)
HEADERS += $$files($$PWD/*.h, true)

!contains(CONFIG, gui) {
  SOURCES -= $$PWD/TSClient/Auth/AuthWindow.cpp
  HEADERS -= $$PWD/TSClient/Auth/AuthWindow.h
}
