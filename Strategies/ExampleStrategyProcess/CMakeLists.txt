cmake_minimum_required(VERSION 3.16)
project(ExampleStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_l2trader_process_strategy(ExampleStrategyProcess
    main.cpp
)

add_custom_command(TARGET ExampleStrategyProcess POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.local/share/L2Trader/Strategies"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.config/L2Trader/Strategies"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:ExampleStrategyProcess>"
            "$ENV{HOME}/.local/share/L2Trader/Strategies/ExampleStrategyProcess"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/example_strategy.json"
            "$ENV{HOME}/.config/L2Trader/Strategies/example_strategy.json"
    COMMENT "Installing ExampleStrategyProcess and example_strategy.json"
)
