cmake_minimum_required(VERSION 3.16)
project(DumpPatternStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_l2trader_process_strategy(DumpPatternStrategyProcess
    main.cpp
)
stage_l2trader_process_strategy(DumpPatternStrategyProcess)
