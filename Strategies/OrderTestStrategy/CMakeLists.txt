cmake_minimum_required(VERSION 3.16)
project(OrderTestStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_l2trader_process_strategy(OrderTestStrategyProcess
    main.cpp
)

add_custom_command(TARGET OrderTestStrategyProcess POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.local/share/L2Trader/Strategies"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.config/L2Trader/Strategies"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:OrderTestStrategyProcess>"
            "$ENV{HOME}/.local/share/L2Trader/Strategies/OrderTestStrategyProcess"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/order_test_strategy.json"
            "$ENV{HOME}/.config/L2Trader/Strategies/order_test_strategy.json"
    COMMENT "Installing OrderTestStrategyProcess and order_test_strategy.json"
)
