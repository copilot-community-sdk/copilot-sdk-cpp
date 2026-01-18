# In-process MCP server example using fastmcpp
# Only built when COPILOT_WITH_FASTMCPP=ON
#
# This example demonstrates running a fastmcpp MCP server and Copilot SDK
# client in the same process, communicating via localhost SSE.

add_executable(mcp_inprocess
    mcp_inprocess.cpp
)

target_link_libraries(mcp_inprocess
    PRIVATE
        copilot_sdk_cpp
        fastmcpp_core
)

set_target_properties(mcp_inprocess PROPERTIES
    FOLDER "Examples/MCP"
)
