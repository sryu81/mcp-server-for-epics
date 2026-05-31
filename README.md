# MCP Server for EPICS

A working well (maybe) MCP server support module for epics.

Just give your controls to your AI agent.

- Exposing EPICS PV as JSON-RPC 2.0 tools
- Protocol version : 2025-11-25 (also accepts 2024-11-05 clients)
- server version : 2.0.0 
- Two deployment modes, one EPICS shared library

## Architecture

    ┌─────────────────────────┐               ┌─────────────────────────┐
    │     epicsMcpServer      │               │       IOC process       │
    │      (standalone)       │               │       (embedded)        │
    └────────────┬────────────┘               └────────────┬────────────┘
                 │                                         │
                 ▼                                         ▼
            stdin/stdout                                TCP port
                                                     (libmicrohttpd)
                 │                                         │
                 └───────────────────┬─────────────────────┘
                                     │
                                     ▼
                                mcpProtocol
                                     │
                                     ▼
                             mcpTools dispatch
                                     │
               ┌─────────────┬───────┴─────┬─────────────┐
               │             │             │             │
               ▼             ▼             ▼             ▼
            pvaOps        caOps          iocsh        system

------------------------------


- libmcpServer.so — shared lib with all logic; linked by both standalone exe and IOC
- mcpServerMain.cpp — 5-line wrapper: main() → mcpServerRunStdio()
- mcpServerRegister.cpp — registers mcpServerStart(port) and mcpServerStop() as iocsh commands via epicsExportRegistrar
- Dependencies: pvaClient nt pvAccess pvAccessCA pvData ca Com microhttpd

## Required library

libmicrohttpd-dev :  library embedding HTTP server functionality

```bash
sudo apt install libmicrohttpd-dev -y
```

## Install

This is a standalone EPICS support module. 

### 1. Clone in $MODULES

```bash
cd $MODULES    <-- your module location
git clone https://github.com/sryu81/mcp-server-for-epics.git mcpServer
cd mcpServer
```

### 2. Set EPICS_BASE in configure/RELEASE

```bash
# Edit configure/RELEASE and set the correct path:
EPICS_BASE = /path/to/your/epics-base
```

EPICS 7.0+ is required (pvAccess/pvaClient/pvData bundled with Base).

### 3. Build

```bash
make
```

Outputs written to `lib/<arch>/` and `bin/<arch>/`:
- `libmcpServer.so.*` — shared library for IOC embedding
- `libmcpServer.a` — static library
- `epicsMcpServer` — standalone executable

### 4. Reference from an IOC application

In your IOC's `configure/RELEASE`:

```makefile
MCPSERVER = /path/to/mcpServer
```

In your IOC's `src/Makefile`:

```makefile
# Link the library
myioc_LIBS += mcpServer

# Install the DBD
myioc_DBD += mcpServer.dbd
```


## Mode 1: Standalone executable (epicsMcpServer)

Stdio transport. Claude Desktop or any MCP host launches it as a subprocess and communicates over
stdin/stdout with newline-delimited JSON-RPC.
```sh
# Both CA and pvAccess enabled (default)
epicsMcpServer

# Disable Channel Access
epicsMcpServer --no-ca

# Disable pvAccess
epicsMcpServer --no-pva

# Help
epicsMcpServer --help
```

Claude Desktop config (~/.config/claude/claude_desktop_config.json on Linux):
```json
{
    "mcpServers": {
    "epics": {
        "command": "/path/to/mcpServer/bin/linux-x86_64/epicsMcpServer",
        "args": [],
        "env": {
        "EPICS_CA_ADDR_LIST": "your-ioc-host",
        "EPICS_PVA_ADDR_LIST": "your-ioc-host"
        }
    }
    }
}
```

## Mode 2: IOC-embedded HTTP server

Streamable HTTP transport using libmicrohttpd. Lives inside a running IOC as libmcpServer.so.
Accepts JSON-RPC via HTTP POST. Has CORS support — any web-based MCP client can connect.

IOC startup script:
```sh
# Load the DBD (registers iocsh commands)
dbLoadDatabase("mcpServer.dbd")
mcpServerRegistrar()

# ... rest of IOC startup ...
iocInit()

# Start MCP server on a TCP port : here use 8080
mcpServerStart(8080)
```
then your IOC will get MCP server running...


HTTP endpoints (mcpTransport.cpp):

| Method | URL | Purpose |
|---|---|---|
| POST | /mcp | JSON-RPC request (max body: 256 KB) |
| GET | /events | SSE endpoint (placeholder; returns endpoint info) |
| OPTIONS | any | CORS preflight |

------------------------------


HTTP client usage:
```sh
# Initialize
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","clientInfo":{"name":"curl","version":"0"}}}'

# List available tools
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}'

# epics_get — read PVs
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"epics_get","arguments":{"pvnames":["MY:TEMP","MY:PRESSURE"]}}}'

# epics_get — Channel Access, custom timeout
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"epics_get","arguments":{"pvnames":["MY:PV"],"protocol":"ca","timeout":10.0}}}'

# epics_put — write a value
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"epics_put","arguments":{"pvname":"MY:SETPOINT","value":42.5}}}'

# epics_monitor — collect changes over 5 seconds
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"epics_monitor","arguments":{"pvnames":["MY:WAVEFORM"],"duration":5.0}}}'

# epics_info — connection metadata
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"epics_info","arguments":{"pvnames":["MY:PV"]}}}'

# epics_version — EPICS build info
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"epics_version","arguments":{}}}'

# epics_status — server status
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"epics_status","arguments":{}}}'

# epics_iocsh — run iocsh command (IOC-embedded only)
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"epics_iocsh","arguments":{"command":"dbl"}}}'

# epics_dbl — list records with optional pattern (IOC-embedded only; stub in standalone)
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":11,"method":"tools/call","params":{"name":"epics_dbl","arguments":{"pattern":"MY:*"}}}'

# epics_dbload — load a database file (IOC-embedded only)
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":12,"method":"tools/call","params":{"name":"epics_dbload","arguments":{"filename":"/path/to/records.db","macros":"P=MY:,R=PV"}}}'

# ping
curl -s -X POST http://ioc-host:8080/mcp \
-H 'Content-Type: application/json' \
-d '{"jsonrpc":"2.0","id":13,"method":"ping","params":{}}'
```

IOC-embedded notes:
- epics_iocsh requires IOC-embedded mode (iocshCmd() linkage); use `dbl` command to list PVs, `dbgf`/`dbpf` to read/write records
- epics_dbl always returns a stub regardless of mode — use epics_iocsh with command `"dbl"` instead
- epics_dbload always returns a stub regardless of mode — use epics_iocsh with command `"dbLoadRecords(\"file.db\",\"P=X:,R=Y\")"` instead
- In standalone mode, epics_iocsh itself is also limited — iocsh commands requiring a loaded IOC database (dbl, dbgf, dbpf) will not work
- HTTP server: one thread per connection (MHD_USE_THREAD_PER_CONNECTION), 120s connection timeout

Stop: 
```sh
# in your IOC shell
epics> mcpServerStop()
```

## MCP Protocol (JSON-RPC 2.0)

Supported methods:

---
| Method | Description |
|---|---|
| initialize | Handshake — returns protocol version, server name/version, capabilities |
| notifications/initialized | Client ack — no response sent |
| tools/list | Returns tool catalog with schemas |
| tools/call | Invoke a tool |
| ping | Returns empty result {} |

All other methods → error -32601 Method not found.

---

## Tools Reference

<details>
  <summary>Click here to expand the section</summary>
  

epics_get, epics_put, epics_monitor, and epics_info accept an optional `"protocol": "pva"` (default) or `"protocol": "ca"` parameter.

**epics_get** : Read one or more PVs.
```json
{
"pvnames": ["MY:TEMP", "MY:PRESSURE"],
"protocol": "pva",
"timeout": 5.0
}
```
- pvnames: array of strings (required)
- timeout: seconds (default 5.0, defined in mcpServer.h:MCP_DEFAULT_TIMEOUT)

**epics_put** : Write a value to a PV.
```json
{
"pvname": "MY:SETPOINT",
"value": 42.5,
"protocol": "ca",
"timeout": 5.0
}
```
- pvname: string (required)
- value: string, integer, or double (required; no arrays/objects)
- timeout: seconds (default 5.0)

**epics_monitor** : Collect all value changes over a time window.
```json
{
"pvnames": ["MY:WAVEFORM"],
"duration": 10.0,
"protocol": "pva"
}
```
- pvnames: array of strings (required)
- duration: seconds, minimum 1.0, maximum 30.0 (MCP_MONITOR_MAX_DURATION); values outside range are clamped

**epics_info** : Connection and metadata for PVs (type, host, alarm status, etc.).

```json
{
"pvnames": ["MY:PV"],
"protocol": "pva",
"timeout": 5.0
}
```
- pvnames: array of strings (required)
- timeout: seconds (default 5.0)

**epics_iocsh (IOC-embedded only)** : Execute any iocsh command; stdout captured via pipe (POSIX only — Windows returns stub).
```json
{
"command": "dbl"
}
```
Returns: 
```json
{"command": "dbl", "status": 0, "output": "MY:PV1\nMY:PV2\n"}
``` 
Buffer capped at 8192 bytes (IOCSH_BUF_SIZE).

**epics_version**
- No parameters. Returns EPICS version string, arch, major/minor/patch, protocol enable flags.

**epics_status**
- No parameters. Returns server name, server version, EPICS version, protocol availability map.

**epics_dbl**
- Always returns a stub. Use epics_iocsh with command `"dbl"` to list PV names (IOC-embedded only).
- pattern: string (optional, accepted but ignored — filtering must be done on the dbl output via epics_iocsh)

**epics_dbload**
- Always returns a stub. Use epics_iocsh with command `"dbLoadRecords(\"file.db\",\"P=X:,R=Y\")"` instead (IOC-embedded only).
- filename: string (required, path to .db or .dbd file)
- macros: string (optional, comma-separated substitutions e.g. `"P=MY:,R=ai1"`)

---
</details>


## Revision History

| Version | Date | Notes |
|---|---|---|
| 2.0.0 | 2026-05-31 | Restructured as standalone EPICS support module (configure/ dir, mcpServerApp/ layout). Upgraded MCP protocol to 2025-11-25: version negotiation in initialize, tool `title` and `required` schema fields, `additionalProperties:false` for no-param tools, `MCP-Protocol-Version` response header, `instructions` in initialize response. HTTP endpoint renamed `/message` → `/mcp` (legacy `/message` still accepted). Backward-compatible version negotiation: server echoes `2024-11-05` back to older clients. Improved tool descriptions and schemas for AI agent discoverability. |
| 1.x | — | Embedded in epics-base/modules. Protocol version 2024-11-05. Stdio + HTTP transports. |