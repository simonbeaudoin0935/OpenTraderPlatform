cmake_minimum_required(VERSION 3.16)
project(HistoricalBarsStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_opentraderplatform_process_strategy(HistoricalBarsStrategyProcess
    main.cpp
)

find_package(Qt6 COMPONENTS Core REQUIRED)
target_link_libraries(HistoricalBarsStrategyProcess PRIVATE Qt6::Core)
stage_opentraderplatform_process_strategy(HistoricalBarsStrategyProcess)
