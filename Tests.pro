CONFIG += c++17
TEMPLATE = subdirs
SUBDIRS = Tests

# Generate logging categories
QMAKE_EXTRA_TARGETS += generate_logging
generate_logging.target = src/Misc/Logging_generated.h
generate_logging.commands = python3 src/Misc/generate_logging_categories.py > src/Misc/Logging_generated.h
generate_logging.depends = $$files(src/*.cpp, true)
PRE_TARGETDEPS += src/Misc/Logging_generated.h