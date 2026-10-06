add_executable(StreamRecoveryTests Clients/StreamRecoveryTests.cpp)
target_link_libraries(StreamRecoveryTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME StreamRecoveryTests COMMAND StreamRecoveryTests)
set_tests_properties(StreamRecoveryTests PROPERTIES
    TIMEOUT 30
    LABELS "local-integration"
)

add_executable(StrategySubscriptionErrorTests Strategy/StrategySubscriptionErrorTests.cpp)
target_link_libraries(StrategySubscriptionErrorTests PRIVATE OpenTraderPlatformStrategySDK Qt6::Test Qt6::Network)
add_test(NAME StrategySubscriptionErrorTests COMMAND StrategySubscriptionErrorTests)
set_tests_properties(StrategySubscriptionErrorTests PROPERTIES
    TIMEOUT 20
    LABELS "local-integration"
)

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
