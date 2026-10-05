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
    add_test(NAME OAuthKeyringUnavailableTests COMMAND OAuthSecurityTests keyringUnavailable)
    set_tests_properties(OAuthKeyringUnavailableTests PROPERTIES
        ENVIRONMENT "OTP_TEST_NO_KEYRING=1"
    )
endif()
