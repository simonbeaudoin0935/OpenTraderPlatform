add_executable(test_recorder_integration main.cpp)

target_link_libraries(test_recorder_integration Qt6::Core Qt6::Test)

add_test(NAME test_recorder_integration COMMAND test_recorder_integration)