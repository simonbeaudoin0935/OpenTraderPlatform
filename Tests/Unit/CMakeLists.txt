add_executable(OAuthSecurityTests
    Security/OAuthSecurityTests.cpp
    ${PROJECT_SOURCE_DIR}/Src/Clients/TSClient/Auth/AuthHandler.cpp
    ${PROJECT_SOURCE_DIR}/Src/Clients/TSClient/Auth/AuthToken.cpp
    ${PROJECT_SOURCE_DIR}/Src/Clients/TSClient/Auth/ClientToken.cpp
    ${PROJECT_SOURCE_DIR}/Src/Misc/SecureStorage.cpp
    ${PROJECT_SOURCE_DIR}/Src/Misc/YubiKeyStorage.cpp
)
target_include_directories(OAuthSecurityTests PRIVATE
    ${PROJECT_SOURCE_DIR}/Src/Clients/TSClient/Auth
    ${PROJECT_SOURCE_DIR}/Src/Misc
)
target_link_libraries(OAuthSecurityTests PRIVATE Qt6::Test Qt6::Network Qt6::Widgets qt6keychain OpenSSL::Crypto)
add_test(NAME OAuthSecurityTests COMMAND OAuthSecurityTests)
set_tests_properties(OAuthSecurityTests PROPERTIES LABELS "unit")

add_executable(BarHistoryBackfillTests
    Core/BarCache/BarHistoryBackfillTests.cpp
    ${PROJECT_SOURCE_DIR}/Src/Core/Models/Bar.cpp
)
target_include_directories(BarHistoryBackfillTests PRIVATE
    ${PROJECT_SOURCE_DIR}/Src/Core/Cache/BarCache
    ${PROJECT_SOURCE_DIR}/Src/Core/Models
)
target_link_libraries(BarHistoryBackfillTests PRIVATE Qt6::Test Qt6::Core)
add_test(NAME BarHistoryBackfillTests COMMAND BarHistoryBackfillTests)
set_tests_properties(BarHistoryBackfillTests PROPERTIES LABELS "unit")

add_executable(BarAggregatorTests Algo/BarAggregatorTests.cpp)
target_link_libraries(BarAggregatorTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME BarAggregatorTests COMMAND BarAggregatorTests)
set_tests_properties(BarAggregatorTests PROPERTIES LABELS "unit")

foreach(test_name BarUtilsTests LiveBarAccumulatorTests)
    add_executable(${test_name} MarketData/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE OpenTraderPlatformCommon Qt6::Test)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "unit")
endforeach()

add_executable(MessageFramingTests Strategy/MessageFramingTests.cpp)
target_link_libraries(MessageFramingTests PRIVATE OpenTraderPlatformStrategySDK Qt6::Test)
add_test(NAME MessageFramingTests COMMAND MessageFramingTests)
set_tests_properties(MessageFramingTests PROPERTIES LABELS "unit")

add_executable(StrategyFrameReaderTests Strategy/StrategyFrameReaderTests.cpp)
target_include_directories(StrategyFrameReaderTests PRIVATE ${PROJECT_SOURCE_DIR}/Src/Strategy)
target_link_libraries(StrategyFrameReaderTests PRIVATE OpenTraderPlatformStrategySDK Qt6::Test)
add_test(NAME StrategyFrameReaderTests COMMAND StrategyFrameReaderTests)
set_tests_properties(StrategyFrameReaderTests PROPERTIES LABELS "unit")

add_executable(TradeStationOrderTestPolicyTests Clients/TradeStationOrderTestPolicyTests.cpp)
target_link_libraries(TradeStationOrderTestPolicyTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME TradeStationOrderTestPolicyTests COMMAND TradeStationOrderTestPolicyTests)
set_tests_properties(TradeStationOrderTestPolicyTests PROPERTIES LABELS "unit")

add_executable(PositionPnLTests Core/PositionPnLTests.cpp)
target_link_libraries(PositionPnLTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME PositionPnLTests COMMAND PositionPnLTests)
set_tests_properties(PositionPnLTests PROPERTIES LABELS "unit")

add_executable(PositionWidgetTests FrontEnd/PositionWidgetTests.cpp)
target_link_libraries(PositionWidgetTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME PositionWidgetTests COMMAND PositionWidgetTests)
set_tests_properties(PositionWidgetTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "unit"
)

add_executable(ShortcutSettingsTests FrontEnd/ShortcutSettingsTests.cpp)
target_link_libraries(ShortcutSettingsTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME ShortcutSettingsTests COMMAND ShortcutSettingsTests)
set_tests_properties(ShortcutSettingsTests PROPERTIES LABELS "unit")

add_executable(IndicatorZoomTests FrontEnd/IndicatorZoomTests.cpp)
target_link_libraries(IndicatorZoomTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
target_compile_options(IndicatorZoomTests PRIVATE -Wno-float-equal)
add_test(NAME IndicatorZoomTests COMMAND IndicatorZoomTests)
set_tests_properties(IndicatorZoomTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "unit"
)

add_executable(TradeStationAuthenticationGateTests
    Clients/TradeStationAuthenticationGateTests.cpp
    ${PROJECT_SOURCE_DIR}/Src/Clients/TSClient/AuthenticatedNetworkAccessManager.cpp
)
target_include_directories(TradeStationAuthenticationGateTests PRIVATE
    ${PROJECT_SOURCE_DIR}/Src/Clients/TSClient
    ${PROJECT_SOURCE_DIR}/Src/Misc
)
target_link_libraries(TradeStationAuthenticationGateTests PRIVATE Qt6::Test Qt6::Network)
add_test(NAME TradeStationAuthenticationGateTests COMMAND TradeStationAuthenticationGateTests)
set_tests_properties(TradeStationAuthenticationGateTests PROPERTIES LABELS "unit")
