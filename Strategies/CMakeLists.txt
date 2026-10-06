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
    get_target_property(_staged ${p_target} OTP_STRATEGY_STAGED)
    if(_staged)
        return()
    endif()
    set_property(TARGET ${p_target} PROPERTY OTP_STRATEGY_STAGED TRUE)
    add_custom_target(stage-${p_target} ALL
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$ENV{HOME}/.local/share/OpenTraderPlatform/Strategies"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:${p_target}>"
                "$ENV{HOME}/.local/share/OpenTraderPlatform/Strategies/${p_target}"
        DEPENDS ${p_target}
        COMMENT "Staging ${p_target}"
        VERBATIM
    )
endfunction()

function(stage_opentraderplatform_strategy_directory p_directory)
    get_property(_targets DIRECTORY "${p_directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_target IN LISTS _targets)
        get_target_property(_type ${_target} TYPE)
        get_target_property(_libraries ${_target} LINK_LIBRARIES)
        if(_type STREQUAL "EXECUTABLE" AND
           ("OpenTraderPlatform::StrategySDK" IN_LIST _libraries OR
            "OpenTraderPlatformStrategySDK" IN_LIST _libraries))
            stage_opentraderplatform_process_strategy(${_target})
        endif()
    endforeach()
    get_property(_subdirectories DIRECTORY "${p_directory}" PROPERTY SUBDIRECTORIES)
    foreach(_directory IN LISTS _subdirectories)
        stage_opentraderplatform_strategy_directory("${_directory}")
    endforeach()
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

stage_opentraderplatform_strategy_directory("${CMAKE_CURRENT_SOURCE_DIR}")
