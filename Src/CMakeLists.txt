# Generate logging categories
set(LOGGING_GENERATED_H "${CMAKE_CURRENT_SOURCE_DIR}/Misc/Logging/Logging_generated.h")
add_custom_command(
    OUTPUT ${LOGGING_GENERATED_H}
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../Utils/generate_logging_categories.py" > ${LOGGING_GENERATED_H}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../Utils/generate_logging_categories.py"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating logging categories"
)
add_custom_target(generate_logging DEPENDS ${LOGGING_GENERATED_H})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Protobuf REQUIRED)

set(L2TRADER_STRATEGY_SDK_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/Public")
set(L2TRADER_STRATEGY_SDK_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/cmake")
set(L2TRADER_STRATEGY_PROTOCOL_PROTO_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategyProtocol/proto/l2trader_strategy_v1.proto"
)
set(PROTOBUF_GENERATE_CPP_APPEND_PATH OFF)
protobuf_generate_cpp(
    L2TRADER_STRATEGY_PROTOCOL_GENERATED_SOURCES
    L2TRADER_STRATEGY_PROTOCOL_GENERATED_HEADERS
    ${L2TRADER_STRATEGY_PROTOCOL_PROTO_FILES}
)
set(L2TRADER_STRATEGY_SDK_INSTALL_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/L2TraderStrategySDK")
set(L2TRADER_STRATEGY_SDK_STAGE_PREFIX "${CMAKE_BINARY_DIR}/strategy-sdk" CACHE PATH
    "Build-tree prefix used by the stage-strategy-sdk target"
)

configure_package_config_file(
    "${L2TRADER_STRATEGY_SDK_CMAKE_DIR}/L2TraderStrategySDKConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/L2TraderStrategySDKConfig.cmake"
    INSTALL_DESTINATION "${L2TRADER_STRATEGY_SDK_INSTALL_CMAKE_DIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/L2TraderStrategySDKConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

# Collect all source files from subdirectories
file(GLOB_RECURSE MISC_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Misc/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Misc/*.h"
)

# Remove GUI-specific shortcut settings from TUI builds
if(NOT ENABLE_GUI)
    list(FILTER MISC_SOURCES EXCLUDE REGEX "ShortcutSettings\\.(cpp|h)$")
endif()

file(GLOB_RECURSE CLIENTS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Clients/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Clients/*.h"
)

# Remove GUI-specific or TUI-specific auth handlers based on build mode
if(ENABLE_GUI)
    list(FILTER CLIENTS_SOURCES EXCLUDE REGEX "TUIAuthHandler\\.(cpp|h)$")
else()
    list(FILTER CLIENTS_SOURCES EXCLUDE REGEX "GUIAuthHandler\\.(cpp|h)$")
endif()

file(GLOB_RECURSE ALGO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Algo/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Algo/*.h"
)

file(GLOB_RECURSE CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/*.h"
)

# Strategy runtime system sources
file(GLOB_RECURSE STRATEGY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Strategy/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Strategy/*.h"
)
list(APPEND STRATEGY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Misc/TimeFrame.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Misc/TimeFrame.h"
)

file(GLOB FRONTEND_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/*.h"
)

# Conditionally add GUI or TUI sources
set(GUI_SOURCES "")
set(TUI_SOURCES "")
set(QCUSTOMPLOT_SOURCES "")
if(ENABLE_GUI)
    file(GLOB_RECURSE GUI_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/*.h"
    )
    set(GUI_UI_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/GUIFrontend.ui"
    )
    # Add qcustomplot sources for GUI builds
    set(QCUSTOMPLOT_SOURCES
        "${CMAKE_SOURCE_DIR}/Lib/QCustomPlot/qcustomplot.cpp"
        "${CMAKE_SOURCE_DIR}/Lib/QCustomPlot/qcustomplot.h"
    )
else()
    file(GLOB_RECURSE TUI_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/TUI/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/TUI/*.h"
    )
endif()

# Create a shared library for the host strategy runtime
add_library(L2TraderStrategy SHARED
    ${STRATEGY_SOURCES}
)

# Add dependency on logging generation
add_dependencies(L2TraderStrategy generate_logging)

add_library(L2TraderStrategySDK SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/ExternalStrategyRuntime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/MessageFraming.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/ProtocolVersion.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/RuntimeEnvironment.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/StrategyDescription.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/StrategyProcessMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/UnixSocketConnection.cpp"
    ${L2TRADER_STRATEGY_PROTOCOL_GENERATED_SOURCES}
)

add_library(L2Trader::StrategySDK ALIAS L2TraderStrategySDK)

set_target_properties(L2TraderStrategySDK PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    EXPORT_NAME StrategySDK
)

target_compile_features(L2TraderStrategySDK PUBLIC cxx_std_23)

target_include_directories(L2TraderStrategySDK PUBLIC
    $<BUILD_INTERFACE:${L2TRADER_STRATEGY_SDK_PUBLIC_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/StrategyProtocol/proto>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(L2TraderStrategySDK PUBLIC
    protobuf::libprotobuf
)

install(TARGETS L2TraderStrategySDK
    EXPORT L2TraderStrategySDKTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT strategy-sdk-runtime
            NAMELINK_COMPONENT strategy-sdk-dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT strategy-sdk-dev
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT strategy-sdk-runtime
)

install(DIRECTORY "${L2TRADER_STRATEGY_SDK_PUBLIC_INCLUDE_DIR}/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT strategy-sdk-dev)

install(FILES ${L2TRADER_STRATEGY_PROTOCOL_GENERATED_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/L2Trader/StrategyProtocol
        COMPONENT strategy-sdk-dev)

install(FILES ${L2TRADER_STRATEGY_PROTOCOL_PROTO_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/l2trader/proto
        COMPONENT strategy-sdk-dev)

install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/L2TraderStrategySDKConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/L2TraderStrategySDKConfigVersion.cmake"
        DESTINATION "${L2TRADER_STRATEGY_SDK_INSTALL_CMAKE_DIR}"
        COMPONENT strategy-sdk-dev)

install(EXPORT L2TraderStrategySDKTargets
        NAMESPACE L2Trader::
        DESTINATION "${L2TRADER_STRATEGY_SDK_INSTALL_CMAKE_DIR}"
        COMPONENT strategy-sdk-dev)

add_custom_target(stage-strategy-sdk
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${L2TRADER_STRATEGY_SDK_STAGE_PREFIX}"
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${L2TRADER_STRATEGY_SDK_STAGE_PREFIX}"
            --component strategy-sdk-runtime
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${L2TRADER_STRATEGY_SDK_STAGE_PREFIX}"
            --component strategy-sdk-dev
    DEPENDS L2TraderStrategySDK
    COMMENT "Staging the out-of-process strategy SDK under ${L2TRADER_STRATEGY_SDK_STAGE_PREFIX}"
)

# Create a library with common sources (excluding strategy which is now separate)
add_library(L2TraderCommon STATIC
    ${MISC_SOURCES}
    ${CLIENTS_SOURCES}
    ${ALGO_SOURCES}
    ${CORE_SOURCES}
    ${FRONTEND_SOURCES}
    ${GUI_SOURCES}
    ${TUI_SOURCES}
    ${GUI_UI_FILES}
    ${QCUSTOMPLOT_SOURCES}
    ${CMAKE_SOURCE_DIR}/Rsc/Resources.qrc
)

# Add dependency on logging generation
add_dependencies(L2TraderCommon generate_logging)

# L2TraderCommon depends on L2TraderStrategy for linking
target_link_libraries(L2TraderCommon PUBLIC L2TraderStrategy)

# Disable strict warnings for third-party qcustomplot code
if(QCUSTOMPLOT_SOURCES)
    set_source_files_properties(${QCUSTOMPLOT_SOURCES} PROPERTIES
        COMPILE_FLAGS "-Wno-error -Wno-shadow -Wno-double-promotion -Wno-float-equal"
    )
endif()

# Set include directories for the strategy library
target_include_directories(L2TraderStrategy PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Misc
    ${CMAKE_CURRENT_SOURCE_DIR}/Misc/Logging
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Auth
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/GetAccounts
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/GetBalances
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/GetOrders
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/StreamOrders
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/StreamPositions
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/OrderExecution
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/OrderExecution/CancelOrder
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/OrderExecution/PlaceOrder
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Stream
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/DBClient
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/BreakingNewsFetcher
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/StreamReceiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/BarReceiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/BarAggregator
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/Level2Receiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/PositionsReceiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/OrdersReceiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/StockScreener
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Models
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Cache
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Cache/BarCache
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Replay
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Replay/OrderEmulator
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/StockPriceChart
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/Tabs
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/TUI
    ${CMAKE_CURRENT_SOURCE_DIR}/Strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/SQL
    ${CMAKE_SOURCE_DIR}/Lib/QCustomPlot
)

# Link Qt libraries to the strategy library
target_link_libraries(L2TraderStrategy PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    L2TraderStrategySDK
    dl
)

if(ENABLE_GUI)
    target_link_libraries(L2TraderStrategy PUBLIC
        Qt6::Gui
        Qt6::Widgets
    )
endif()

# Set include directories for the common library
target_include_directories(L2TraderCommon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Misc
    ${CMAKE_CURRENT_SOURCE_DIR}/Misc/Logging
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Auth
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/GetAccounts
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/GetBalances
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/GetOrders
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/StreamOrders
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Brokerage/StreamPositions
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/OrderExecution
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/OrderExecution/CancelOrder
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/OrderExecution/PlaceOrder
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/TSClient/Stream
    ${CMAKE_CURRENT_SOURCE_DIR}/Clients/DBClient
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/BreakingNewsFetcher
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/StreamReceiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/BarReceiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/BarAggregator
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/Level2Receiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/PositionsReceiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/OrdersReceiver
    ${CMAKE_CURRENT_SOURCE_DIR}/Algo/StockScreener
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Models
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Cache
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Cache/BarCache
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Replay
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Replay/OrderEmulator
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/StockPriceChart
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/Tabs
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/GUI/Dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd/TUI
    ${CMAKE_CURRENT_SOURCE_DIR}/Strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/SQL
    ${CMAKE_SOURCE_DIR}/Lib/QCustomPlot
)

# Include qcustomplot as SYSTEM to suppress external library warnings
target_include_directories(L2TraderCommon SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/Lib/QCustomPlot
)

# Disable null-dereference error for this target because MOC-generated code includes
# qcustomplot.h without SYSTEM treatment, triggering false positives in template code.
target_compile_options(L2TraderCommon PRIVATE -Wno-error=null-dereference -Wno-error=unused-but-set-variable -Wno-float-equal)

# Link Qt libraries to the common library
target_link_libraries(L2TraderCommon PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    databento::databento
    dl
)

if(ENABLE_GUI)
    target_link_libraries(L2TraderCommon PUBLIC
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
    )
else()
    target_link_libraries(L2TraderCommon PUBLIC
        ${CURSES_LIBRARIES}
    )
    target_include_directories(L2TraderCommon PUBLIC
        ${CURSES_INCLUDE_DIRS}
    )
endif()

# Main L2Trader executable
add_executable(L2Trader
    main.cpp
)

target_link_libraries(L2Trader PRIVATE L2TraderCommon)

# LTTng UST: link tracepoint provider library when enabled
if(LTTNG_ENABLED)
    target_include_directories(L2TraderCommon PUBLIC ${LTTNG_UST_INCLUDE_DIRS})
    target_link_libraries(L2TraderCommon PUBLIC ${LTTNG_UST_LIBRARIES})
endif()
