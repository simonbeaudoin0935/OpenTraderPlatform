function(add_l2trader_process_strategy p_target)
    add_executable(${p_target} ${ARGN})
    target_compile_features(${p_target} PRIVATE cxx_std_23)

    if(TARGET L2Trader::StrategySDK)
        target_link_libraries(${p_target} PRIVATE L2Trader::StrategySDK)
    else()
        find_package(L2TraderStrategySDK CONFIG REQUIRED)
        target_link_libraries(${p_target} PRIVATE L2Trader::StrategySDK)
    endif()

    set_target_properties(${p_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endfunction()

add_subdirectory(ExampleStrategyProcess)
add_subdirectory(CrashTestStrategy)
add_subdirectory(HistoricalBarsStrategy)
add_subdirectory(OrderTestStrategy)
add_subdirectory(DumpPatternStrategy)
