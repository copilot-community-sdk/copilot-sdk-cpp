# copilot-sdk-cpp

C++ SDK for interacting with the Copilot CLI agent runtime (JSON-RPC over stdio or TCP).

[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue)]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()

## Build

Requirements: CMake 3.20+ and a C++20 compiler.

```sh
cmake -S . -B build
cmake --build build --config Release
```

## Tests

Unit tests:

```sh
ctest --test-dir build -C Release --output-on-failure
```

E2E tests (real Copilot CLI):
- Require `copilot` to be installed and authenticated.
- To disable E2E tests in CI/non-interactive runs, set `COPILOT_SDK_CPP_SKIP_E2E=1`.

Snapshot conformance tests (optional):
- Require Python 3 and the upstream `copilot-sdk` snapshots directory.
- Install Python deps: `python -m pip install --user -r tests/snapshot_tests/requirements.txt`
- Enable with `-DCOPILOT_BUILD_SNAPSHOT_TESTS=ON` and set `-DCOPILOT_SDK_CPP_SNAPSHOT_DIR=...` if auto-detection fails.
- Run: `cmake --build build --target run_snapshot_tests --config Release`

## Custom Tools

Custom tools must be provided when creating or resuming a session. The SDK registers tool handlers locally and sends tool definitions to the server:

```cpp
// Define your tool
copilot::Tool calc_tool;
calc_tool.name = "calculator";
calc_tool.description = "Perform math calculations";
calc_tool.parameters_schema = copilot::json{
    {"type", "object"},
    {"properties", {{"expression", {{"type", "string"}}}}},
    {"required", {"expression"}}
};
calc_tool.handler = [](const copilot::ToolInvocation& inv) {
    copilot::ToolResultObject result;
    // Handle the tool call...
    return result;
};

// Pass tools when creating the session
copilot::SessionConfig config;
config.tools = {calc_tool};
auto session = client.create_session(config).get();

// Or when resuming an existing session
copilot::ResumeSessionConfig resume_config;
resume_config.tools = {calc_tool};
auto session = client.resume_session(session_id, resume_config).get();
```

See `examples/tools.cpp` and `examples/resume_with_tools.cpp` for complete examples.

## Install / Package

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=./install
cmake --build build --config Release
cmake --install build --config Release
```

## Related Projects

| Project | Language | Description |
|---------|----------|-------------|
| [claude-agent-sdk-cpp](https://github.com/0xeb/claude-agent-sdk-cpp) | C++ | C++ port of the Claude Agent SDK |
| [claude-agent-sdk-dotnet](https://github.com/0xeb/claude-agent-sdk-dotnet) | C# | .NET port of the Claude Agent SDK |
| [fastmcpp](https://github.com/0xeb/fastmcpp) | C++ | C++ port of FastMCP for building MCP servers |

## License

Copyright 2025 Elias Bachaalany

Licensed under the MIT License. See `LICENSE` for details.

This is a C++ port of [copilot-sdk](https://github.com/github/copilot-sdk) by GitHub.
