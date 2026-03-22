cmake_minimum_required(VERSION 3.16)
project(HistoricalBarsStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_l2trader_process_strategy(HistoricalBarsStrategyProcess
    main.cpp
)

find_package(Qt6 COMPONENTS Core REQUIRED)
target_link_libraries(HistoricalBarsStrategyProcess PRIVATE Qt6::Core)

add_custom_command(TARGET HistoricalBarsStrategyProcess POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.local/share/L2Trader/Strategies"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.config/L2Trader/Strategies"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:HistoricalBarsStrategyProcess>"
            "$ENV{HOME}/.local/share/L2Trader/Strategies/HistoricalBarsStrategyProcess"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/historical_bars_strategy.json"
            "$ENV{HOME}/.config/L2Trader/Strategies/historical_bars_strategy.json"
    COMMENT "Installing HistoricalBarsStrategyProcess and historical_bars_strategy.json"
)
