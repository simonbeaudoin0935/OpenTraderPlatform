add_executable(OAuthSecurityTests
    OAuthSecurityTests.cpp
    ../Src/Clients/TSClient/Auth/AuthHandler.cpp
    ../Src/Clients/TSClient/Auth/AuthToken.cpp
    ../Src/Clients/TSClient/Auth/ClientToken.cpp
    ../Src/Misc/SecureStorage.cpp
    ../Src/Misc/YubiKeyStorage.cpp
)
target_include_directories(OAuthSecurityTests PRIVATE
    ../Src/Clients/TSClient/Auth
    ../Src/Misc
)
target_link_libraries(OAuthSecurityTests PRIVATE Qt6::Test Qt6::Network Qt6::Widgets qt6keychain OpenSSL::Crypto)
add_test(NAME OAuthSecurityTests COMMAND OAuthSecurityTests)

add_executable(BarHistoryBackfillTests
    BarHistoryBackfillTests.cpp
    ../Src/Core/Models/Bar.cpp
)
target_include_directories(BarHistoryBackfillTests PRIVATE
    ../Src/Core/Cache/BarCache
    ../Src/Core/Models
)
target_link_libraries(BarHistoryBackfillTests PRIVATE Qt6::Test Qt6::Core)
add_test(NAME BarHistoryBackfillTests COMMAND BarHistoryBackfillTests)

add_executable(IndicatorZoomTests IndicatorZoomTests.cpp)
target_link_libraries(IndicatorZoomTests PRIVATE OpenTraderPlatformCommon Qt6::Test)
add_test(NAME IndicatorZoomTests COMMAND IndicatorZoomTests)
set_tests_properties(IndicatorZoomTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(TradeStationAuthenticationGateTests
    TradeStationAuthenticationGateTests.cpp
    ../Src/Clients/TSClient/AuthenticatedNetworkAccessManager.cpp
)
target_include_directories(TradeStationAuthenticationGateTests PRIVATE
    ../Src/Clients/TSClient
    ../Src/Misc
)
target_link_libraries(TradeStationAuthenticationGateTests PRIVATE Qt6::Test Qt6::Network)
add_test(NAME TradeStationAuthenticationGateTests COMMAND TradeStationAuthenticationGateTests)

add_test(NAME OAuthKeyringRoundTripTests COMMAND OAuthSecurityTests keyringRoundTrip)
set_tests_properties(OAuthKeyringRoundTripTests PROPERTIES
    ENVIRONMENT "OTP_TEST_KEYRING_ROUNDTRIP=1"
    LABELS "integration"
)

if(UNIX AND NOT APPLE)
    add_test(NAME YubiKeyStorageSessionTests COMMAND OAuthSecurityTests yubiKeySession)
    set_tests_properties(YubiKeyStorageSessionTests PROPERTIES
        ENVIRONMENT "OTP_TEST_YUBIKEY=1"
    )
    add_test(NAME YubiKeyLockedResetTests COMMAND OAuthSecurityTests yubiKeyLockedReset)
    set_tests_properties(YubiKeyLockedResetTests PROPERTIES
        ENVIRONMENT "OTP_TEST_YUBIKEY=1"
    )
    add_test(NAME OAuthKeyringUnavailableTests COMMAND OAuthSecurityTests keyringUnavailable)
    set_tests_properties(OAuthKeyringUnavailableTests PROPERTIES
        ENVIRONMENT "OTP_TEST_NO_KEYRING=1"
    )
endif()
