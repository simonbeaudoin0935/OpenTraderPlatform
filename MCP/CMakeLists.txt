include(GNUInstallDirs)

function(add_l2trader_mcp_tool p_target)
    add_executable(${p_target} ${ARGN})
    target_compile_features(${p_target} PRIVATE cxx_std_23)
    target_link_libraries(${p_target} PRIVATE Qt6::Core Qt6::Network L2TraderMCPCommon)
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

add_library(L2TraderMCPCommon STATIC
    Common/PlatformControlClient.cpp
)
target_compile_features(L2TraderMCPCommon PRIVATE cxx_std_23)
target_link_libraries(L2TraderMCPCommon PUBLIC Qt6::Core Qt6::Network)
target_include_directories(L2TraderMCPCommon PUBLIC
    "${CMAKE_SOURCE_DIR}/Src"
    "${CMAKE_CURRENT_SOURCE_DIR}/Common"
)

add_l2trader_mcp_tool(L2TraderMCPServer
    MCPServer/main.cpp
)
set_target_properties(L2TraderMCPServer PROPERTIES OUTPUT_NAME "l2trader-mcp-server")

add_l2trader_mcp_tool(L2TraderControlCLI
    ControlCLI/main.cpp
)
set_target_properties(L2TraderControlCLI PROPERTIES OUTPUT_NAME "l2trader-ctl")
