cmake_minimum_required(VERSION 3.16)
project(OrderTestStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_opentraderplatform_process_strategy(OrderTestStrategyProcess
    main.cpp
)

find_package(Qt6 COMPONENTS Core REQUIRED)
target_link_libraries(OrderTestStrategyProcess PRIVATE Qt6::Core)
