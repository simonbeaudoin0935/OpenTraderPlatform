cmake_minimum_required(VERSION 3.16)
project(CrashTestStrategyProcess VERSION 1.0.0 LANGUAGES CXX)

add_opentraderplatform_process_strategy(CrashTestStrategyProcess
    main.cpp
)
