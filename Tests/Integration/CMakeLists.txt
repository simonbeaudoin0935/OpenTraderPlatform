add_executable(TradeStationApiSmokeTests TradeStation/TradeStationApiSmokeTests.cpp)
target_link_libraries(TradeStationApiSmokeTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME TradeStationApiSmokeTests COMMAND TradeStationApiSmokeTests
    discoversSimulationAccount readsAccountBalances readsQuoteSnapshot readsHistoricalRange
    reportsInvalidSymbol receivesPositionSnapshotAndCloses receivesQuoteStreamAndCloses)
set_tests_properties(TradeStationApiSmokeTests PROPERTIES
    LABELS "tradestation-api"
    TIMEOUT 240
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
)

add_test(NAME TradeStationPaperOrderTests COMMAND TradeStationApiSmokeTests paperOrderLifecycle)
set_tests_properties(TradeStationPaperOrderTests PROPERTIES
    LABELS "tradestation-orders"
    TIMEOUT 240
    RUN_SERIAL TRUE
    SKIP_RETURN_CODE 77
)

add_executable(TradeStationOptInGuardTests TradeStation/TradeStationOptInGuardTests.cpp)
target_link_libraries(TradeStationOptInGuardTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
target_compile_definitions(TradeStationOptInGuardTests PRIVATE
    OTP_TRADESTATION_TEST_EXECUTABLE="$<TARGET_FILE:TradeStationApiSmokeTests>"
)
add_dependencies(TradeStationOptInGuardTests TradeStationApiSmokeTests)
add_test(NAME TradeStationOptInGuardTests COMMAND TradeStationOptInGuardTests)
set_tests_properties(TradeStationOptInGuardTests PROPERTIES TIMEOUT 30 LABELS "local-integration")

add_executable(StreamRecoveryTests Clients/StreamRecoveryTests.cpp)
target_link_libraries(StreamRecoveryTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME StreamRecoveryTests COMMAND StreamRecoveryTests)
set_tests_properties(StreamRecoveryTests PROPERTIES
    TIMEOUT 30
    LABELS "local-integration"
)

add_executable(BarCachePersistenceTests Core/BarCachePersistenceTests.cpp)
target_link_libraries(BarCachePersistenceTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME BarCachePersistenceTests COMMAND BarCachePersistenceTests)
set_tests_properties(BarCachePersistenceTests PROPERTIES TIMEOUT 45 LABELS "local-integration")

add_executable(OrderEmulatorTests Core/Replay/OrderEmulatorTests.cpp)
target_link_libraries(OrderEmulatorTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME OrderEmulatorTests COMMAND OrderEmulatorTests)
set_tests_properties(OrderEmulatorTests PROPERTIES TIMEOUT 45 LABELS "local-integration")

add_executable(RiskManagerTests Algo/RiskManagerTests.cpp)
target_link_libraries(RiskManagerTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME RiskManagerTests COMMAND RiskManagerTests)
set_tests_properties(RiskManagerTests PROPERTIES TIMEOUT 45 LABELS "local-integration")

add_executable(StrategySubscriptionErrorTests Strategy/StrategySubscriptionErrorTests.cpp)
target_link_libraries(StrategySubscriptionErrorTests PRIVATE OpenTraderPlatformStrategySDK Qt6::Test Qt6::Network)
add_test(NAME StrategySubscriptionErrorTests COMMAND StrategySubscriptionErrorTests)
set_tests_properties(StrategySubscriptionErrorTests PROPERTIES
    TIMEOUT 20
    LABELS "local-integration"
)

add_executable(StrategyTransportTests Strategy/StrategyTransportTests.cpp)
target_link_libraries(StrategyTransportTests PRIVATE OpenTraderPlatformStrategySDK Qt6::Test Qt6::Network)
add_test(NAME StrategyTransportTests COMMAND StrategyTransportTests)
set_tests_properties(StrategyTransportTests PROPERTIES TIMEOUT 30 LABELS "local-integration")

add_test(NAME OAuthKeyringRoundTripTests COMMAND OAuthSecurityTests keyringRoundTrip)
set_tests_properties(OAuthKeyringRoundTripTests PROPERTIES
    ENVIRONMENT "OTP_TEST_KEYRING_ROUNDTRIP=1"
    LABELS "local-integration"
)

if(UNIX AND NOT APPLE)
    add_executable(StartupRestoreTests Core/StartupRestoreTests.cpp)
    target_include_directories(StartupRestoreTests PRIVATE ${PROJECT_SOURCE_DIR}/Src/Misc)
    target_link_libraries(StartupRestoreTests PRIVATE Qt6::Test)
    target_compile_definitions(StartupRestoreTests PRIVATE
        OTP_PLATFORM_EXECUTABLE="$<TARGET_FILE:OpenTraderPlatform>"
    )
    add_dependencies(StartupRestoreTests OpenTraderPlatform)
    add_test(NAME StartupRestoreTests COMMAND StartupRestoreTests)
    set_tests_properties(StartupRestoreTests PROPERTIES LABELS "local-integration")

    add_test(NAME YubiKeyStorageSessionTests COMMAND OAuthSecurityTests yubiKeySession)
    set_tests_properties(YubiKeyStorageSessionTests PROPERTIES
        ENVIRONMENT "OTP_TEST_YUBIKEY=1"
        LABELS "local-integration"
    )
    add_test(NAME YubiKeyLockedResetTests COMMAND OAuthSecurityTests yubiKeyLockedReset)
    set_tests_properties(YubiKeyLockedResetTests PROPERTIES
        ENVIRONMENT "OTP_TEST_YUBIKEY=1"
        LABELS "local-integration"
    )
    add_test(NAME OAuthKeyringUnavailableTests COMMAND OAuthSecurityTests keyringUnavailable)
    set_tests_properties(OAuthKeyringUnavailableTests PROPERTIES
        ENVIRONMENT "OTP_TEST_NO_KEYRING=1"
        LABELS "local-integration"
    )
endif()
