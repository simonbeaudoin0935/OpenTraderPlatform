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
    ${CMAKE_SOURCE_DIR}/src/Misc
    ${CMAKE_SOURCE_DIR}/src/Clients
    ${CMAKE_SOURCE_DIR}/src/Clients/FMPClient
    ${CMAKE_SOURCE_DIR}/src/Clients/FMPClient/Filters
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Auth
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetAccounts
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetBalances
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetOrders
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/StreamOrders
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/StreamPositions
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/Bars
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/GetQuoteSnapshots
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/MockStream
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/StreamMarketDepthQuote
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/StreamQuote
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution/CancelOrder
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution/PlaceOrder
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Stream
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/Core/Cache
    ${CMAKE_SOURCE_DIR}/src/Core/Cache/BarCache
)

# Collect sources needed for the test
file(GLOB_RECURSE BARCACHE_TEST_MISC_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Misc/*.cpp"
)
file(GLOB_RECURSE BARCACHE_TEST_CLIENTS_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Clients/*.cpp"
)
file(GLOB_RECURSE BARCACHE_TEST_CACHE_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Core/Cache/*.cpp"
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

# FMPClient Unit Test
add_executable(test_fmpclient
    FMPClientUnit/TestFMPClient.cpp
    FMPClientUnit/TestFMPClient.h
    FMPClientUnit/main.cpp
)

target_link_libraries(test_fmpclient PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    Qt6::Gui
)

target_include_directories(test_fmpclient PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Misc
    ${CMAKE_SOURCE_DIR}/src/Clients
    ${CMAKE_SOURCE_DIR}/src/Clients/FMPClient
    ${CMAKE_SOURCE_DIR}/src/Clients/FMPClient/Filters
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Auth
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetAccounts
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetBalances
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetOrders
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/StreamOrders
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/StreamPositions
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/Bars
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/GetQuoteSnapshots
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/MockStream
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/StreamMarketDepthQuote
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/StreamQuote
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution/CancelOrder
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution/PlaceOrder
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Stream
)

# Collect sources needed for the test
file(GLOB_RECURSE FMPCLIENT_TEST_MISC_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Misc/*.cpp"
)
file(GLOB_RECURSE FMPCLIENT_TEST_CLIENTS_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Clients/*.cpp"
)

# Exclude GUI-specific sources (both .cpp and .h)
list(FILTER FMPCLIENT_TEST_CLIENTS_SOURCES EXCLUDE REGEX "AuthWindow\\.(cpp|h)$")

target_sources(test_fmpclient PRIVATE
    ${FMPCLIENT_TEST_MISC_SOURCES}
    ${FMPCLIENT_TEST_CLIENTS_SOURCES}
)

target_compile_definitions(test_fmpclient PRIVATE UNIT_TESTING)
target_compile_options(test_fmpclient PRIVATE -Og)

add_test(NAME FMPClientTest COMMAND test_fmpclient)

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
    ${CMAKE_SOURCE_DIR}/src/Misc
    ${CMAKE_SOURCE_DIR}/src/Clients
    ${CMAKE_SOURCE_DIR}/src/Clients/FMPClient
    ${CMAKE_SOURCE_DIR}/src/Clients/FMPClient/Filters
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Auth
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetAccounts
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetBalances
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetOrders
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/StreamOrders
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/StreamPositions
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/Bars
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/GetQuoteSnapshots
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/MockStream
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/StreamMarketDepthQuote
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/StreamQuote
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution/CancelOrder
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution/PlaceOrder
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Stream
)

# Collect sources needed for the test
file(GLOB_RECURSE TSCLIENT_TEST_MISC_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Misc/*.cpp"
)
file(GLOB_RECURSE TSCLIENT_TEST_CLIENTS_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Clients/*.cpp"
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
    Qt6::Network
    Qt6::Sql
    Qt6::Test
    Qt6::Gui
)

target_include_directories(test_RunUpDetector PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Misc
    ${CMAKE_SOURCE_DIR}/src/Clients
    ${CMAKE_SOURCE_DIR}/src/Clients/FMPClient
    ${CMAKE_SOURCE_DIR}/src/Clients/FMPClient/Filters
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Auth
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetAccounts
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetBalances
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/GetOrders
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/StreamOrders
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Brokerage/StreamPositions
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/Bars
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/GetQuoteSnapshots
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/MockStream
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/StreamMarketDepthQuote
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/MarketData/StreamQuote
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution/CancelOrder
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/OrderExecution/PlaceOrder
    ${CMAKE_SOURCE_DIR}/src/Clients/TSClient/Stream
    ${CMAKE_SOURCE_DIR}/src/Core/Cache
    ${CMAKE_SOURCE_DIR}/src/Core/Cache/BarCache
    ${CMAKE_SOURCE_DIR}/src/Algo
    ${CMAKE_SOURCE_DIR}/src/Algo/RunUpDetector
)

# Collect sources needed for the test
file(GLOB_RECURSE RUNUPDETECTOR_TEST_MISC_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Misc/*.cpp"
)
file(GLOB_RECURSE RUNUPDETECTOR_TEST_CLIENTS_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Clients/*.cpp"
)
file(GLOB_RECURSE RUNUPDETECTOR_TEST_CACHE_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Core/Cache/*.cpp"
)
file(GLOB_RECURSE RUNUPDETECTOR_TEST_ALGO_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Algo/RunUpDetector/*.cpp"
)

# Exclude GUI-specific sources (both .cpp and .h)
list(FILTER RUNUPDETECTOR_TEST_CLIENTS_SOURCES EXCLUDE REGEX "AuthWindow\\.(cpp|h)$")

target_sources(test_RunUpDetector PRIVATE
    ${RUNUPDETECTOR_TEST_MISC_SOURCES}
    ${RUNUPDETECTOR_TEST_CLIENTS_SOURCES}
    ${RUNUPDETECTOR_TEST_CACHE_SOURCES}
    ${RUNUPDETECTOR_TEST_ALGO_SOURCES}
)

target_compile_definitions(test_RunUpDetector PRIVATE UNIT_TESTING)
target_compile_options(test_RunUpDetector PRIVATE -Og)

add_test(NAME RunUpDetectorTest COMMAND test_RunUpDetector)
