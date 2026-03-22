cmake_minimum_required(VERSION 3.16)
project(DumpPatternStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_l2trader_process_strategy(DumpPatternStrategyProcess
    main.cpp
)

add_custom_command(TARGET DumpPatternStrategyProcess POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.local/share/L2Trader/Strategies"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.config/L2Trader/Strategies"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:DumpPatternStrategyProcess>"
            "$ENV{HOME}/.local/share/L2Trader/Strategies/DumpPatternStrategyProcess"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/dump_pattern_strategy.json"
            "$ENV{HOME}/.config/L2Trader/Strategies/dump_pattern_strategy.json"
    COMMENT "Installing DumpPatternStrategyProcess and dump_pattern_strategy.json"
)
