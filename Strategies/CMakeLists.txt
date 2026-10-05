function(add_opentraderplatform_process_strategy p_target)
    add_executable(${p_target} ${ARGN})
    target_compile_features(${p_target} PRIVATE cxx_std_23)

    if(TARGET OpenTraderPlatform::StrategySDK)
        target_link_libraries(${p_target} PRIVATE OpenTraderPlatform::StrategySDK)
    else()
        find_package(OpenTraderPlatformStrategySDK CONFIG REQUIRED)
        target_link_libraries(${p_target} PRIVATE OpenTraderPlatform::StrategySDK)
    endif()

    set_target_properties(${p_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endfunction()

function(stage_opentraderplatform_process_strategy p_target)
    add_custom_command(TARGET ${p_target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.local/share/OpenTraderPlatform/Strategies"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:${p_target}>"
                "$ENV{HOME}/.local/share/OpenTraderPlatform/Strategies/${p_target}"
        COMMENT "Installing ${p_target}"
    )
endfunction()

add_subdirectory(Examples)

option(BUILD_STRATEGY_TEST_HARNESSES "Build strategy crash/order test harnesses" OFF)
if(BUILD_STRATEGY_TEST_HARNESSES)
    add_subdirectory(Tests)
endif()

option(BUILD_PRIVATE_STRATEGIES "Build the ignored Strategies/Privates checkout" OFF)
if(BUILD_PRIVATE_STRATEGIES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Privates/CMakeLists.txt")
        message(FATAL_ERROR "BUILD_PRIVATE_STRATEGIES is ON, but Strategies/Privates/CMakeLists.txt was not found")
    endif()
    add_subdirectory(Privates)
endif()
