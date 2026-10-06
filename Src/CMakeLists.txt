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
# Prefer the upstream package config where available; some distributions only
# ship CMake's FindProtobuf module with libprotobuf-dev.
find_package(Protobuf QUIET CONFIG)
if(NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED MODULE)
endif()

set(OPENTRADERPLATFORM_STRATEGY_SDK_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/Public")
set(OPENTRADERPLATFORM_STRATEGY_SDK_VERSION "2.0.0")
set(OPENTRADERPLATFORM_STRATEGY_SDK_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/cmake")
set(OPENTRADERPLATFORM_STRATEGY_PROTOCOL_PROTO_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategyProtocol/proto/opentraderplatform_strategy_v1.proto"
)
set(OPENTRADERPLATFORM_STRATEGY_PROTOCOL_GENERATED_SOURCES)
set(OPENTRADERPLATFORM_STRATEGY_PROTOCOL_GENERATED_HEADERS)
foreach(_proto_file ${OPENTRADERPLATFORM_STRATEGY_PROTOCOL_PROTO_FILES})
    get_filename_component(_proto_abs "${_proto_file}" ABSOLUTE)
    get_filename_component(_proto_dir "${_proto_abs}" DIRECTORY)
    get_filename_component(_proto_name "${_proto_file}" NAME_WE)
    file(RELATIVE_PATH _proto_rel_dir "${CMAKE_CURRENT_SOURCE_DIR}" "${_proto_dir}")

    set(_generated_header "${CMAKE_CURRENT_BINARY_DIR}/${_proto_rel_dir}/${_proto_name}.pb.h")
    set(_generated_source "${CMAKE_CURRENT_BINARY_DIR}/${_proto_rel_dir}/${_proto_name}.pb.cc")

    list(APPEND OPENTRADERPLATFORM_STRATEGY_PROTOCOL_GENERATED_HEADERS "${_generated_header}")
    list(APPEND OPENTRADERPLATFORM_STRATEGY_PROTOCOL_GENERATED_SOURCES "${_generated_source}")

    add_custom_command(
        OUTPUT "${_generated_header}" "${_generated_source}"
        COMMAND protobuf::protoc
        ARGS --experimental_allow_proto3_optional
             --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
             -I "${CMAKE_CURRENT_SOURCE_DIR}"
             "${_proto_abs}"
        DEPENDS "${_proto_abs}" protobuf::protoc
        COMMENT "Running cpp protocol buffer compiler on ${_proto_file}"
        VERBATIM
    )
endforeach()
set_source_files_properties(${OPENTRADERPLATFORM_STRATEGY_PROTOCOL_GENERATED_SOURCES}
                            ${OPENTRADERPLATFORM_STRATEGY_PROTOCOL_GENERATED_HEADERS}
                            PROPERTIES GENERATED TRUE)
set(OPENTRADERPLATFORM_STRATEGY_SDK_INSTALL_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/OpenTraderPlatformStrategySDK")
set(OPENTRADERPLATFORM_STRATEGY_SDK_STAGE_PREFIX "${CMAKE_BINARY_DIR}/strategy-sdk" CACHE PATH
    "Build-tree prefix used by the stage-strategy-sdk target"
)

configure_package_config_file(
    "${OPENTRADERPLATFORM_STRATEGY_SDK_CMAKE_DIR}/OpenTraderPlatformStrategySDKConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/OpenTraderPlatformStrategySDKConfig.cmake"
    INSTALL_DESTINATION "${OPENTRADERPLATFORM_STRATEGY_SDK_INSTALL_CMAKE_DIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/OpenTraderPlatformStrategySDKConfigVersion.cmake"
    VERSION ${OPENTRADERPLATFORM_STRATEGY_SDK_VERSION}
    COMPATIBILITY SameMinorVersion
)

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

# GUI-only frontend sources.
set(GUI_SOURCES "")
set(QCUSTOMPLOT_SOURCES "")
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

# Create a shared library for the host strategy runtime
add_library(OpenTraderPlatformStrategy SHARED
    ${STRATEGY_SOURCES}
)

# Add dependency on logging generation
add_dependencies(OpenTraderPlatformStrategy generate_logging)

add_library(OpenTraderPlatformStrategySDK SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/ExternalStrategyRuntime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/MessageFraming.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/ProtocolVersion.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/RuntimeEnvironment.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/StrategyDescription.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/StrategyProcessMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrategySDK/UnixSocketConnection.cpp"
    ${OPENTRADERPLATFORM_STRATEGY_PROTOCOL_GENERATED_SOURCES}
)

add_library(OpenTraderPlatform::StrategySDK ALIAS OpenTraderPlatformStrategySDK)

set_target_properties(OpenTraderPlatformStrategySDK PROPERTIES
    VERSION ${OPENTRADERPLATFORM_STRATEGY_SDK_VERSION}
    SOVERSION 2
    EXPORT_NAME StrategySDK
)

target_compile_features(OpenTraderPlatformStrategySDK PUBLIC cxx_std_23)

target_include_directories(OpenTraderPlatformStrategySDK PUBLIC
    $<BUILD_INTERFACE:${OPENTRADERPLATFORM_STRATEGY_SDK_PUBLIC_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/StrategyProtocol/proto>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(OpenTraderPlatformStrategySDK PUBLIC
    protobuf::libprotobuf
)

install(TARGETS OpenTraderPlatformStrategySDK
    EXPORT OpenTraderPlatformStrategySDKTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT strategy-sdk-runtime
            NAMELINK_COMPONENT strategy-sdk-dev
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT strategy-sdk-dev
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT strategy-sdk-runtime
)

install(DIRECTORY "${OPENTRADERPLATFORM_STRATEGY_SDK_PUBLIC_INCLUDE_DIR}/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT strategy-sdk-dev)

install(FILES ${OPENTRADERPLATFORM_STRATEGY_PROTOCOL_GENERATED_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/OpenTraderPlatform/StrategyProtocol
        COMPONENT strategy-sdk-dev)

install(FILES ${OPENTRADERPLATFORM_STRATEGY_PROTOCOL_PROTO_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/opentraderplatform/proto
        COMPONENT strategy-sdk-dev)

install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/OpenTraderPlatformStrategySDKConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/OpenTraderPlatformStrategySDKConfigVersion.cmake"
        DESTINATION "${OPENTRADERPLATFORM_STRATEGY_SDK_INSTALL_CMAKE_DIR}"
        COMPONENT strategy-sdk-dev)

install(EXPORT OpenTraderPlatformStrategySDKTargets
        NAMESPACE OpenTraderPlatform::
        DESTINATION "${OPENTRADERPLATFORM_STRATEGY_SDK_INSTALL_CMAKE_DIR}"
        COMPONENT strategy-sdk-dev)

add_custom_target(stage-strategy-sdk
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${OPENTRADERPLATFORM_STRATEGY_SDK_STAGE_PREFIX}"
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${OPENTRADERPLATFORM_STRATEGY_SDK_STAGE_PREFIX}"
            --component strategy-sdk-runtime
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${OPENTRADERPLATFORM_STRATEGY_SDK_STAGE_PREFIX}"
            --component strategy-sdk-dev
    DEPENDS OpenTraderPlatformStrategySDK
    COMMENT "Staging the out-of-process strategy SDK under ${OPENTRADERPLATFORM_STRATEGY_SDK_STAGE_PREFIX}"
)

# Create a library with common sources (excluding strategy which is now separate)
add_library(OpenTraderPlatformCommon STATIC
    ${MISC_SOURCES}
    ${CLIENTS_SOURCES}
    ${ALGO_SOURCES}
    ${CORE_SOURCES}
    ${FRONTEND_SOURCES}
    ${GUI_SOURCES}
    ${GUI_UI_FILES}
    ${QCUSTOMPLOT_SOURCES}
    ${CMAKE_SOURCE_DIR}/Rsc/Resources.qrc
)

# Add dependency on logging generation
add_dependencies(OpenTraderPlatformCommon generate_logging)

# OpenTraderPlatformCommon depends on OpenTraderPlatformStrategy for linking
target_link_libraries(OpenTraderPlatformCommon PUBLIC OpenTraderPlatformStrategy)

# Disable strict warnings for third-party qcustomplot code
if(QCUSTOMPLOT_SOURCES)
    set_source_files_properties(${QCUSTOMPLOT_SOURCES} PROPERTIES
        COMPILE_FLAGS "-Wno-error -Wno-shadow -Wno-double-promotion -Wno-float-equal"
    )
endif()

# Set include directories for the strategy library
target_include_directories(OpenTraderPlatformStrategy PUBLIC
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
    ${CMAKE_CURRENT_SOURCE_DIR}/Strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/SQL
    ${CMAKE_SOURCE_DIR}/Lib/QCustomPlot
)

# Link Qt libraries to the strategy library
target_link_libraries(OpenTraderPlatformStrategy PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Gui
    Qt6::Widgets
    OpenTraderPlatformStrategySDK
    dl
)

# Set include directories for the common library
target_include_directories(OpenTraderPlatformCommon PUBLIC
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
    ${CMAKE_CURRENT_SOURCE_DIR}/Strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/SQL
    ${CMAKE_SOURCE_DIR}/Lib/QCustomPlot
)

# Include qcustomplot as SYSTEM to suppress external library warnings
target_include_directories(OpenTraderPlatformCommon SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/Lib/QCustomPlot
)

# Disable null-dereference error for this target because MOC-generated code includes
# qcustomplot.h without SYSTEM treatment, triggering false positives in template code.
target_compile_options(OpenTraderPlatformCommon PRIVATE -Wno-error=null-dereference -Wno-error=unused-but-set-variable -Wno-float-equal)

# Link Qt libraries to the common library
target_link_libraries(OpenTraderPlatformCommon PUBLIC
    qt6keychain
    OpenSSL::Crypto
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    databento::databento
    dl
)

# Main OpenTraderPlatform executable
add_executable(OpenTraderPlatform
    main.cpp
)

target_link_libraries(OpenTraderPlatform PRIVATE OpenTraderPlatformCommon)

add_executable(DatabentoLiveProbe
    DatabentoLiveProbe.cpp
)

target_link_libraries(DatabentoLiveProbe PRIVATE OpenTraderPlatformCommon)

# LTTng UST: link tracepoint provider library when enabled
if(LTTNG_ENABLED)
    target_include_directories(OpenTraderPlatformCommon PUBLIC ${LTTNG_UST_INCLUDE_DIRS})
    target_link_libraries(OpenTraderPlatformCommon PUBLIC ${LTTNG_UST_LIBRARIES})
endif()
