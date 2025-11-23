# Generate logging categories
set(LOGGING_GENERATED_H "${CMAKE_CURRENT_SOURCE_DIR}/Misc/Logging_generated.h")
add_custom_command(
    OUTPUT ${LOGGING_GENERATED_H}
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/Misc/generate_logging_categories.py" > ${LOGGING_GENERATED_H}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Misc/generate_logging_categories.py"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating logging categories"
)
add_custom_target(generate_logging DEPENDS ${LOGGING_GENERATED_H})

# Collect all source files from subdirectories
file(GLOB_RECURSE MISC_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/Misc/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Misc/*.h"
)

file(GLOB_RECURSE CLIENTS_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/Clients/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Clients/*.h"
)

file(GLOB_RECURSE ALGO_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/Algo/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Algo/*.h"
)

file(GLOB_RECURSE CORE_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/*.h"
)

file(GLOB FRONTEND_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/*.h"
)

# Conditionally add GUI or TUI sources
set(GUI_SOURCES "")
set(TUI_SOURCES "")
if(ENABLE_GUI)
    file(GLOB_RECURSE GUI_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/*.h"
    )
    set(GUI_UI_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/GUIFrontend.ui"
    )
else()
    file(GLOB_RECURSE TUI_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/TUI/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/TUI/*.h"
    )
    # Remove GUI-incompatible sources from Clients when GUI is disabled
    list(FILTER CLIENTS_SOURCES EXCLUDE REGEX "AuthWindow\\.(cpp|h)$")
endif()

# Create a library with common sources
add_library(L2TraderCommon STATIC
    ${MISC_SOURCES}
    ${CLIENTS_SOURCES}
    ${ALGO_SOURCES}
    ${CORE_SOURCES}
    ${FRONTEND_SOURCES}
    ${GUI_SOURCES}
    ${TUI_SOURCES}
    ${GUI_UI_FILES}
    ${CMAKE_SOURCE_DIR}/Resources/Resources.qrc
)

# Add dependency on logging generation
add_dependencies(L2TraderCommon generate_logging)

# Set include directories for the library
target_include_directories(L2TraderCommon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Misc
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/FMPClient
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/FMPClient/Filters
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Auth
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/GetAccounts
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/GetBalances
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/GetOrders
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/StreamOrders
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/StreamPositions
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/MarketData
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/MarketData/Bars
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/MarketData/GetQuoteSnapshots
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/MarketData/MockStream
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/MarketData/StreamMarketDepthQuote
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/MarketData/StreamQuote
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/OrderExecution
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/OrderExecution/CancelOrder
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/OrderExecution/PlaceOrder
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Stream
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/BreakingNewsFetcher
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/MarketDepthQuoteReceiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/PositionsReceiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/RunUpDetector
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/StockScreener
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Cache
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Cache/BarCache
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/Gauge
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/StockPriceChart
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/TUI
    ${CMAKE_CURRENT_SOURCE_DIR}/Recorder
)

# Link Qt libraries to the common library
target_link_libraries(L2TraderCommon PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Gui
)

if(ENABLE_GUI)
    target_link_libraries(L2TraderCommon PUBLIC
        Qt6::Widgets
        Qt6::Charts
        Qt6::WebEngineWidgets
    )
endif()

# Main L2Trader executable
add_executable(L2Trader
    main.cpp
)

target_link_libraries(L2Trader PRIVATE L2TraderCommon)

# Recorder executable (optional)
if(BUILD_RECORDER)
    add_executable(L2Trader_Recorder
        Recorder/main.cpp
        Recorder/RecorderLogic.cpp
        Recorder/LiveStreamDB.cpp
        Recorder/StatusReporter.cpp
        Recorder/RecorderUtils.cpp
    )

    target_link_libraries(L2Trader_Recorder PRIVATE L2TraderCommon)
endif()
