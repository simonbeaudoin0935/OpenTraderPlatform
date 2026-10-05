cmake_minimum_required(VERSION 3.16)
project(ExampleStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_opentraderplatform_process_strategy(ExampleStrategyProcess
    main.cpp
)
stage_opentraderplatform_process_strategy(ExampleStrategyProcess)
