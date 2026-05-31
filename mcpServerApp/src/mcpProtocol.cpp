
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <yajl_gen.h>

#include "mcpProtocol.h"
#include "mcpServer.h"
#include "mcpJson.h"
#include "mcpTools.h"

static void mcpProtocolFlush(yajl_gen gen, McpWriteFn writeFn, void *writeCtx)
{
    const unsigned char *buf;
    size_t len;
    yajl_gen_get_buf(gen, &buf, &len);
    if (writeFn)
        writeFn(writeCtx, (const char *)buf, len);
    yajl_gen_free(gen);
}

static void sendError(long long id, int code, const char *message,
                      McpWriteFn writeFn, void *writeCtx)
{
    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "jsonrpc", "2.0");
    mcpGenString(gen, "id");
    yajl_gen_integer(gen, id);
    mcpGenString(gen, "error");
    yajl_gen_map_open(gen);
    mcpGenKeyInt(gen, "code", code);
    mcpGenKeyString(gen, "message", message);
    yajl_gen_map_close(gen);
    yajl_gen_map_close(gen);
    mcpProtocolFlush(gen, writeFn, writeCtx);
}

/* All protocol versions this server can speak */
static const char * const s_supported_versions[] = {
    "2025-11-25",
    "2024-11-05",
    NULL
};

static void handleInitialize(McpJsonValue *root, long long id,
                             McpWriteFn writeFn, void *writeCtx)
{
    /* Version negotiation: echo client version if we support it,
       otherwise respond with our latest. Client decides whether to disconnect. */
    McpJsonValue *params = mcpJsonMapGet(root, "params");
    const char *clientVer = params
        ? mcpJsonGetString(mcpJsonMapGet(params, "protocolVersion"))
        : NULL;
    const char *agreedVer = MCP_PROTOCOL_VERSION;
    if (clientVer) {
        int i;
        for (i = 0; s_supported_versions[i]; i++) {
            if (strcmp(clientVer, s_supported_versions[i]) == 0) {
                agreedVer = s_supported_versions[i];
                break;
            }
        }
    }

    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "jsonrpc", "2.0");
    mcpGenString(gen, "id");
    yajl_gen_integer(gen, id);

    mcpGenString(gen, "result");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "protocolVersion", agreedVer);

    mcpGenString(gen, "capabilities");
    yajl_gen_map_open(gen);
    mcpGenString(gen, "tools");
    yajl_gen_map_open(gen);
    mcpGenKeyBool(gen, "listChanged", 0);
    yajl_gen_map_close(gen);
    yajl_gen_map_close(gen);

    mcpGenString(gen, "serverInfo");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "name", MCP_SERVER_NAME);
    mcpGenKeyString(gen, "title", "EPICS MCP Server");
    mcpGenKeyString(gen, "version", MCP_SERVER_VERSION);
    yajl_gen_map_close(gen);

    mcpGenKeyString(gen, "instructions",
        "Use epics_get/epics_put to read/write EPICS PVs. "
        "Use epics_monitor to collect changes over a time window. "
        "Use epics_info for connection metadata and type info. "
        "To list all PV names, use epics_iocsh with command 'dbl' — "
        "epics_dbl is a stub and always redirects to this. "
        "epics_iocsh/epics_dbl/epics_dbload require IOC-embedded mode.");

    yajl_gen_map_close(gen);
    yajl_gen_map_close(gen);
    mcpProtocolFlush(gen, writeFn, writeCtx);

    mcpState.initialized = 1;
}

static void handleToolsList(long long id, McpWriteFn writeFn, void *writeCtx)
{
    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "jsonrpc", "2.0");
    mcpGenString(gen, "id");
    yajl_gen_integer(gen, id);

    mcpGenString(gen, "result");
    yajl_gen_map_open(gen);
    mcpGenString(gen, "tools");
    mcpToolsWriteList(gen);
    yajl_gen_map_close(gen);

    yajl_gen_map_close(gen);
    mcpProtocolFlush(gen, writeFn, writeCtx);
}

static void handleToolsCall(McpJsonValue *root, long long id,
                            McpWriteFn writeFn, void *writeCtx)
{
    McpJsonValue *params = mcpJsonMapGet(root, "params");
    if (!params) {
        sendError(id, -32602, "Missing params", writeFn, writeCtx);
        return;
    }

    const char *toolName = mcpJsonGetString(mcpJsonMapGet(params, "name"));
    if (!toolName) {
        sendError(id, -32602, "Missing tool name", writeFn, writeCtx);
        return;
    }

    const McpToolDef *tool = mcpToolsFind(toolName);
    if (!tool) {
        sendError(id, -32601, "Tool not found", writeFn, writeCtx);
        return;
    }

    McpJsonValue *arguments = mcpJsonMapGet(params, "arguments");

    yajl_gen gen = yajl_gen_alloc(NULL);
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "jsonrpc", "2.0");
    mcpGenString(gen, "id");
    yajl_gen_integer(gen, id);

    mcpGenString(gen, "result");
    yajl_gen_map_open(gen);

    mcpGenString(gen, "content");
    yajl_gen_array_open(gen);
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "text");

    mcpGenString(gen, "text");

    yajl_gen toolGen = yajl_gen_alloc(NULL);
    int rc = tool->handler(arguments, toolGen);

    const unsigned char *toolBuf;
    size_t toolLen;
    yajl_gen_get_buf(toolGen, &toolBuf, &toolLen);

    if (rc == 0) {
        yajl_gen_string(gen, toolBuf, toolLen);
    } else {
        mcpGenString(gen, "Error executing tool");
    }
    yajl_gen_free(toolGen);

    yajl_gen_map_close(gen);
    yajl_gen_array_close(gen);

    if (rc != 0)
        mcpGenKeyBool(gen, "isError", 1);

    yajl_gen_map_close(gen);
    yajl_gen_map_close(gen);
    mcpProtocolFlush(gen, writeFn, writeCtx);
}

void mcpProtocolInit(void)
{
    mcpToolsInit();
}

void mcpProtocolHandleRequest(const char *line, McpWriteFn writeFn, void *writeCtx)
{
    size_t len = strlen(line);
    if (len == 0) return;

    McpJsonValue *root = mcpJsonParse(line, len);
    if (!root || root->type != MCP_JSON_MAP) {
        sendError(0, -32700, "Parse error", writeFn, writeCtx);
        mcpJsonFree(root);
        return;
    }

    const char *method = mcpJsonGetString(mcpJsonMapGet(root, "method"));
    McpJsonValue *idVal = mcpJsonMapGet(root, "id");
    long long id = idVal ? mcpJsonGetInt(idVal) : 0;

    if (!method) {
        sendError(id, -32600, "Invalid request: missing method", writeFn, writeCtx);
        mcpJsonFree(root);
        return;
    }

    if (strcmp(method, "initialize") == 0) {
        handleInitialize(root, id, writeFn, writeCtx);
    }
    else if (strcmp(method, "notifications/initialized") == 0) {
        /* no response */
    }
    else if (strcmp(method, "tools/list") == 0) {
        handleToolsList(id, writeFn, writeCtx);
    }
    else if (strcmp(method, "tools/call") == 0) {
        handleToolsCall(root, id, writeFn, writeCtx);
    }
    else if (strcmp(method, "ping") == 0) {
        yajl_gen gen = yajl_gen_alloc(NULL);
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "jsonrpc", "2.0");
        mcpGenString(gen, "id");
        yajl_gen_integer(gen, id);
        mcpGenString(gen, "result");
        yajl_gen_map_open(gen);
        yajl_gen_map_close(gen);
        yajl_gen_map_close(gen);
        mcpProtocolFlush(gen, writeFn, writeCtx);
    }
    else {
        sendError(id, -32601, "Method not found", writeFn, writeCtx);
    }

    mcpJsonFree(root);
}

/* --- Buffer-based response: allocates string caller must free() --- */

struct RespBuf {
    char *buf;
    size_t len;
    size_t cap;
};

static void respBufWrite(void *ctx, const char *data, size_t len)
{
    struct RespBuf *rb = (struct RespBuf *)ctx;
    size_t needed = rb->len + len + 1;
    if (needed > rb->cap) {
        if (!rb->cap) rb->cap = 4096;
        while (rb->cap < needed) rb->cap *= 2;
        rb->buf = (char *)realloc(rb->buf, rb->cap);
    }
    memcpy(rb->buf + rb->len, data, len);
    rb->len += len;
    rb->buf[rb->len] = '\0';
}

char *mcpProtocolHandleRequestStr(const char *line, size_t *outLen)
{
    struct RespBuf rb = {0};
    mcpProtocolHandleRequest(line, respBufWrite, &rb);
    if (outLen)
        *outLen = rb.len;
    return rb.buf;
}
