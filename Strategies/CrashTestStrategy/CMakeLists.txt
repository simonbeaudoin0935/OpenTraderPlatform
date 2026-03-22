cmake_minimum_required(VERSION 3.16)
project(CrashTestStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_l2trader_process_strategy(CrashTestStrategyProcess
    main.cpp
)

add_custom_command(TARGET CrashTestStrategyProcess POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.local/share/L2Trader/Strategies"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.config/L2Trader/Strategies"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:CrashTestStrategyProcess>"
            "$ENV{HOME}/.local/share/L2Trader/Strategies/CrashTestStrategyProcess"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/crash_test_strategy.json"
            "$ENV{HOME}/.config/L2Trader/Strategies/crash_test_strategy.json"
    COMMENT "Installing CrashTestStrategyProcess and crash_test_strategy.json"
)
