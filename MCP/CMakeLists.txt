include(GNUInstallDirs)

function(add_opentraderplatform_mcp_tool p_target)
    add_executable(${p_target} ${ARGN})
    target_compile_features(${p_target} PRIVATE cxx_std_23)
    target_link_libraries(${p_target} PRIVATE Qt6::Core Qt6::Network OpenTraderPlatformMCPCommon)
    target_include_directories(${p_target} PRIVATE
        "${CMAKE_SOURCE_DIR}/Src"
        "${CMAKE_CURRENT_SOURCE_DIR}/Common"
    )
    set_target_properties(${p_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    install(TARGETS ${p_target}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT runtime
    )
endfunction()

add_library(OpenTraderPlatformMCPCommon STATIC
    Common/PlatformControlClient.cpp
)
target_compile_features(OpenTraderPlatformMCPCommon PRIVATE cxx_std_23)
target_link_libraries(OpenTraderPlatformMCPCommon PUBLIC Qt6::Core Qt6::Network)
target_include_directories(OpenTraderPlatformMCPCommon PUBLIC
    "${CMAKE_SOURCE_DIR}/Src"
    "${CMAKE_CURRENT_SOURCE_DIR}/Common"
)

add_opentraderplatform_mcp_tool(OpenTraderPlatformMCPServer
    MCPServer/main.cpp
)
set_target_properties(OpenTraderPlatformMCPServer PROPERTIES OUTPUT_NAME "opentraderplatform-mcp-server")
