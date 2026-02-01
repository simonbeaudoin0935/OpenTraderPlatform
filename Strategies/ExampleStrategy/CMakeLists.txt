cmake_minimum_required(VERSION 3.16)
project(ExampleStrategy VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -Wall -Wextra")

# Find Qt6
find_package(Qt6 COMPONENTS Core REQUIRED)

# Get path to L2Trader source (this CMakeLists assumes it's being run from Strategies/ExampleStrategy)
set(L2TRADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../Src")

# Create shared library for the strategy plugin
add_library(ExampleStrategy SHARED
    ExampleStrategy.cpp
    ExampleStrategy.h
)

# Link Qt6
target_link_libraries(ExampleStrategy PRIVATE Qt6::Core)

# Include all L2Trader headers with explicit subdirectories
target_include_directories(ExampleStrategy PRIVATE 
    "${L2TRADER_SRC}"
    "${L2TRADER_SRC}/Algo"
    "${L2TRADER_SRC}/Algo/MarketDepthQuoteReceiver"
    "${L2TRADER_SRC}/Algo/OrdersReceiver"
    "${L2TRADER_SRC}/Algo/PositionsReceiver"
    "${L2TRADER_SRC}/Clients"
    "${L2TRADER_SRC}/Clients/TSClient"
    "${L2TRADER_SRC}/Clients/TSClient/Auth"
    "${L2TRADER_SRC}/Clients/TSClient/Brokerage"
    "${L2TRADER_SRC}/Clients/TSClient/Brokerage/GetAccounts"
    "${L2TRADER_SRC}/Clients/TSClient/Brokerage/GetBalances"
    "${L2TRADER_SRC}/Clients/TSClient/Brokerage/GetOrders"
    "${L2TRADER_SRC}/Clients/TSClient/Brokerage/StreamOrders"
    "${L2TRADER_SRC}/Clients/TSClient/Brokerage/StreamPositions"
    "${L2TRADER_SRC}/Clients/TSClient/MarketData"
    "${L2TRADER_SRC}/Clients/TSClient/MarketData/Bars"
    "${L2TRADER_SRC}/Clients/TSClient/MarketData/GetQuoteSnapshots"
    "${L2TRADER_SRC}/Clients/TSClient/MarketData/StreamMarketDepthQuote"
    "${L2TRADER_SRC}/Clients/TSClient/OrderExecution"
    "${L2TRADER_SRC}/Clients/TSClient/OrderExecution/CancelOrder"
    "${L2TRADER_SRC}/Clients/TSClient/OrderExecution/PlaceOrder"
    "${L2TRADER_SRC}/Clients/TSClient/Stream"
    "${L2TRADER_SRC}/Core"
    "${L2TRADER_SRC}/Core/Cache"
    "${L2TRADER_SRC}/Core/Cache/BarCache"
    "${L2TRADER_SRC}/FrontEnd"
    "${L2TRADER_SRC}/FrontEnd/GUI"
    "${L2TRADER_SRC}/FrontEnd/GUI/Gauge"
    "${L2TRADER_SRC}/FrontEnd/GUI/StockPriceChart"
    "${L2TRADER_SRC}/FrontEnd/GUI/StrategiesTab"
    "${L2TRADER_SRC}/FrontEnd/GUI/Tabs"
    "${L2TRADER_SRC}/FrontEnd/TUI"
    "${L2TRADER_SRC}/Misc"
    "${L2TRADER_SRC}/Recorder"
    "${L2TRADER_SRC}/SQL"
    "${L2TRADER_SRC}/Strategy"
)

# Set output directory
set_target_properties(ExampleStrategy PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    PREFIX ""
    SUFFIX ".so"
)

message(STATUS "Building ExampleStrategy plugin")
message(STATUS "  L2Trader SDK path: ${L2TRADER_SRC}")
message(STATUS "  Output: ${CMAKE_BINARY_DIR}/lib/ExampleStrategy.so")
message(STATUS "  Install: mkdir -p ~/.local/share/L2Trader/strategies/ && cp ${CMAKE_BINARY_DIR}/lib/ExampleStrategy.so ~/.local/share/L2Trader/strategies/")

