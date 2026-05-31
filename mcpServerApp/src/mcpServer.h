#ifndef INC_mcpServer_H
#define INC_mcpServer_H

#include <stdio.h>
#include "mcpJson.h"

#define MCP_PROTOCOL_VERSION "2024-11-05"
#define MCP_SERVER_NAME "EPICS Base MCP Server"
#define MCP_SERVER_VERSION "2.0.0"

#define MCP_MAX_LINE 65536
#define MCP_DEFAULT_TIMEOUT 5.0
#define MCP_MONITOR_MAX_DURATION 30.0

typedef struct {
    int initialized;
    int shutdownRequested;
    int caEnabled;
    int pvaEnabled;
} McpServerState;

extern McpServerState mcpState;

const char *mcpGetProtocol(const McpJsonValue *params);


#endif /* INC_mcpServer_H */
