# Common test settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Remove GUI_ENABLED from all tests - they should run without GUI
remove_definitions(-DGUI_ENABLED)

# BarCache Unit Test
add_executable(test_barcache
    BarCacheUnit/TestBarCache.cpp
    BarCacheUnit/TestBarCache.h
    BarCacheUnit/main.cpp
)

target_link_libraries(test_barcache PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Test
    Qt6::Gui
)

target_include_directories(test_barcache PRIVATE
    ${CMAKE_SOURCE_DIR}/Src/Misc
    ${CMAKE_SOURCE_DIR}/Src/Clients
    ${CMAKE_SOURCE_DIR}/Src/Clients/FMPClient
    ${CMAKE_SOURCE_DIR}/Src/Clients/FMPClient/Filters
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Auth
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/GetAccounts
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/GetBalances
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/GetOrders
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/StreamOrders
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/StreamPositions
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/Bars
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/GetQuoteSnapshots
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/MockStream
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/StreamMarketDepthQuote
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/StreamQuote
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/OrderExecution
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/OrderExecution/CancelOrder
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/OrderExecution/PlaceOrder
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Stream
    ${CMAKE_SOURCE_DIR}/Src/Core
    ${CMAKE_SOURCE_DIR}/Src/Core/Cache
    ${CMAKE_SOURCE_DIR}/Src/Core/Cache/BarCache
)

# Collect sources needed for the test
file(GLOB_RECURSE BARCACHE_TEST_MISC_SOURCES
    "${CMAKE_SOURCE_DIR}/Src/Misc/*.cpp"
)
file(GLOB_RECURSE BARCACHE_TEST_CLIENTS_SOURCES
    "${CMAKE_SOURCE_DIR}/Src/Clients/*.cpp"
)
file(GLOB_RECURSE BARCACHE_TEST_CACHE_SOURCES
    "${CMAKE_SOURCE_DIR}/Src/Core/Cache/*.cpp"
)

# Exclude GUI-specific sources (both .cpp and .h)
list(FILTER BARCACHE_TEST_CLIENTS_SOURCES EXCLUDE REGEX "AuthWindow\\.(cpp|h)$")

target_sources(test_barcache PRIVATE
    ${BARCACHE_TEST_MISC_SOURCES}
    ${BARCACHE_TEST_CLIENTS_SOURCES}
    ${BARCACHE_TEST_CACHE_SOURCES}
)

target_compile_definitions(test_barcache PRIVATE UNIT_TESTING)
target_compile_options(test_barcache PRIVATE -Og)

add_test(NAME BarCacheTest COMMAND test_barcache)

# TSClient Unit Test
add_executable(test_tradestationclient
    TSClientUnit/TestTSClient.cpp
    TSClientUnit/TestTSClient.h
    TSClientUnit/main.cpp
)

target_link_libraries(test_tradestationclient PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    Qt6::Gui
)

target_include_directories(test_tradestationclient PRIVATE
    ${CMAKE_SOURCE_DIR}/Src/Misc
    ${CMAKE_SOURCE_DIR}/Src/Clients
    ${CMAKE_SOURCE_DIR}/Src/Clients/FMPClient
    ${CMAKE_SOURCE_DIR}/Src/Clients/FMPClient/Filters
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Auth
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/GetAccounts
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/GetBalances
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/GetOrders
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/StreamOrders
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/StreamPositions
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/Bars
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/GetQuoteSnapshots
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/MockStream
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/StreamMarketDepthQuote
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/StreamQuote
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/OrderExecution
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/OrderExecution/CancelOrder
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/OrderExecution/PlaceOrder
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Stream
)

# Collect sources needed for the test
file(GLOB_RECURSE TSCLIENT_TEST_MISC_SOURCES
    "${CMAKE_SOURCE_DIR}/Src/Misc/*.cpp"
)
file(GLOB_RECURSE TSCLIENT_TEST_CLIENTS_SOURCES
    "${CMAKE_SOURCE_DIR}/Src/Clients/*.cpp"
)

# Exclude GUI-specific sources (both .cpp and .h)
list(FILTER TSCLIENT_TEST_CLIENTS_SOURCES EXCLUDE REGEX "AuthWindow\\.(cpp|h)$")

target_sources(test_tradestationclient PRIVATE
    ${TSCLIENT_TEST_MISC_SOURCES}
    ${TSCLIENT_TEST_CLIENTS_SOURCES}
)

target_compile_definitions(test_tradestationclient PRIVATE UNIT_TESTING)
target_compile_options(test_tradestationclient PRIVATE -Og)

add_test(NAME TSClientTest COMMAND test_tradestationclient)

# RunUpDetector Unit Test
add_executable(test_RunUpDetector
    RunUpDetectorUnit/TestRunUpDetector.cpp
    RunUpDetectorUnit/TestRunUpDetector.h
    RunUpDetectorUnit/main.cpp
)

target_link_libraries(test_RunUpDetector PRIVATE
    Qt6::Core
    Qt6::Concurrent
    Qt6::Network
    Qt6::Sql
    Qt6::Test
    Qt6::Gui
)

target_include_directories(test_RunUpDetector PRIVATE
    ${CMAKE_SOURCE_DIR}/Src/Misc
    ${CMAKE_SOURCE_DIR}/Src/Clients
    ${CMAKE_SOURCE_DIR}/Src/Clients/FMPClient
    ${CMAKE_SOURCE_DIR}/Src/Clients/FMPClient/Filters
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Auth
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/GetAccounts
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/GetBalances
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/GetOrders
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/StreamOrders
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Brokerage/StreamPositions
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/Bars
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/GetQuoteSnapshots
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/MockStream
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/StreamMarketDepthQuote
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/MarketData/StreamQuote
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/OrderExecution
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/OrderExecution/CancelOrder
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/OrderExecution/PlaceOrder
    ${CMAKE_SOURCE_DIR}/Src/Clients/TSClient/Stream
    ${CMAKE_SOURCE_DIR}/Src/Core
    ${CMAKE_SOURCE_DIR}/Src/Core/Cache
    ${CMAKE_SOURCE_DIR}/Src/Core/Cache/BarCache
    ${CMAKE_SOURCE_DIR}/Src/FrontEnd
    ${CMAKE_SOURCE_DIR}/Src/Algo
    ${CMAKE_SOURCE_DIR}/Src/Algo/MarketDepthQuoteReceiver
    ${CMAKE_SOURCE_DIR}/Src/Algo/OrdersReceiver
    ${CMAKE_SOURCE_DIR}/Src/Algo/PositionsReceiver
    ${CMAKE_SOURCE_DIR}/Src/Algo/RunUpDetector
)

# Collect sources needed for the test
file(GLOB_RECURSE RUNUPDETECTOR_TEST_MISC_SOURCES
    "${CMAKE_SOURCE_DIR}/Src/Misc/*.cpp"
)
file(GLOB_RECURSE RUNUPDETECTOR_TEST_CLIENTS_SOURCES
    "${CMAKE_SOURCE_DIR}/Src/Clients/*.cpp"
)
file(GLOB_RECURSE RUNUPDETECTOR_TEST_CACHE_SOURCES
    "${CMAKE_SOURCE_DIR}/Src/Core/Cache/*.cpp"
)
file(GLOB_RECURSE RUNUPDETECTOR_TEST_CORE_SOURCES
    "${CMAKE_SOURCE_DIR}/Src/Core/*.cpp"
)
file(GLOB_RECURSE RUNUPDETECTOR_TEST_ALGO_SOURCES
    "${CMAKE_SOURCE_DIR}/Src/Algo/RunUpDetector/*.cpp"
)

# Exclude GUI-specific sources (both .cpp and .h)
list(FILTER RUNUPDETECTOR_TEST_CLIENTS_SOURCES EXCLUDE REGEX "AuthWindow\\.(cpp|h)$")
# Exclude Core sources that are not needed or cause GUI linkage issues
list(FILTER RUNUPDETECTOR_TEST_CORE_SOURCES EXCLUDE REGEX "Cache/")

target_sources(test_RunUpDetector PRIVATE
    ${RUNUPDETECTOR_TEST_MISC_SOURCES}
    ${RUNUPDETECTOR_TEST_CLIENTS_SOURCES}
    ${RUNUPDETECTOR_TEST_CACHE_SOURCES}
    ${RUNUPDETECTOR_TEST_CORE_SOURCES}
    ${RUNUPDETECTOR_TEST_ALGO_SOURCES}
)

target_compile_definitions(test_RunUpDetector PRIVATE UNIT_TESTING)
target_compile_options(test_RunUpDetector PRIVATE -Og)

add_test(NAME RunUpDetectorTest COMMAND test_RunUpDetector)

# Recorder Integration Test
add_executable(test_recorder_integration
    RecorderIntegrationTest/main.cpp
)

target_link_libraries(test_recorder_integration PRIVATE
    Qt6::Core
    Qt6::Test
)

target_compile_definitions(test_recorder_integration PRIVATE UNIT_TESTING)
target_compile_options(test_recorder_integration PRIVATE -Og)

add_test(NAME RecorderIntegrationTest COMMAND test_recorder_integration)
