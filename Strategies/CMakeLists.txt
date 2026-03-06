# Build strategy plugins
# Each strategy subdirectory has its own CMakeLists.txt that builds the .so plugin

add_subdirectory(ExampleStrategy)
add_subdirectory(CrashTestStrategy)
add_subdirectory(HistoricalBarsStrategy)
add_subdirectory(OrderTestStrategy)
add_subdirectory(DumpPatternStrategy)
