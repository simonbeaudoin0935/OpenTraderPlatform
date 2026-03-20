include(GNUInstallDirs)

function(add_l2trader_mcp_tool p_target)
    add_executable(${p_target} ${ARGN})
    target_compile_features(${p_target} PRIVATE cxx_std_23)
    target_link_libraries(${p_target} PRIVATE Qt6::Core)
    set_target_properties(${p_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    install(TARGETS ${p_target}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT runtime
    )
endfunction()

add_l2trader_mcp_tool(L2TraderMCPServer
    MCPServer/main.cpp
)
set_target_properties(L2TraderMCPServer PROPERTIES OUTPUT_NAME "l2trader-mcp-server")

add_l2trader_mcp_tool(L2TraderControlCLI
    ControlCLI/main.cpp
)
set_target_properties(L2TraderControlCLI PROPERTIES OUTPUT_NAME "l2trader-ctl")
