# In-Process MCP Server Example

This example demonstrates running a [fastmcpp](https://github.com/0xeb/fastmcpp) MCP server and Copilot SDK client in the **same process**.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Single Process                        │
│                                                          │
│  ┌──────────────────┐      SSE       ┌───────────────┐  │
│  │  fastmcpp Server │◄──────────────►│  Copilot SDK  │  │
│  │  (background     │   localhost    │    Client     │  │
│  │   thread)        │                │               │  │
│  │                  │                │               │  │
│  │  Tools:          │                │  Connects as  │  │
│  │  - secret_vault  │                │  remote MCP   │  │
│  │  - calculator    │                │  server       │  │
│  │  - system_info   │                │               │  │
│  └──────────────────┘                └───────────────┘  │
└─────────────────────────────────────────────────────────┘
```

## How It Works

- fastmcpp SSE server runs as a **background thread** on `localhost:18080`
- Copilot SDK connects to it as a remote MCP server
- **Single process** - no child processes spawned
- Communication uses localhost HTTP (SSE transport)

## Building

This example requires the `COPILOT_WITH_FASTMCPP` CMake option:

```bash
cmake -B build -DCOPILOT_WITH_FASTMCPP=ON
cmake --build build
```

The build system will:
1. First try `find_package(fastmcpp)` for a local installation
2. If not found, automatically fetch fastmcpp from GitHub via FetchContent

## Running

```bash
./build/examples/mcp/inprocess/mcp_inprocess
```

The example starts an interactive REPL. Try these prompts:

- `Look up the INTEROP_TEST secret`
- `Calculate 42 * 17 using the calculator`
- `Get system info from the MCP server`

Type `quit` to exit.

## Tools Provided

| Tool | Description |
|------|-------------|
| `secret_vault_lookup` | Look up secrets (API_TOKEN, DATABASE_URL, ENCRYPTION_KEY, INTEROP_TEST) |
| `logged_calculator` | Perform add/multiply operations with logging |
| `get_system_info` | Get MCP server information |

## Use Cases

This pattern is useful when you want to:
- Expose native C++ functionality to Copilot via MCP tools
- Run everything in a single process without external dependencies
- Single binary deployment with embedded MCP capabilities
