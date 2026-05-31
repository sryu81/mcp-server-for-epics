#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <microhttpd.h>

#include "mcpTransport.h"
#include "mcpProtocol.h"
#include "mcpServer.h"

/* --- Stdio transport --- */

int mcpTransportInit(void)
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    return 0;
}

int mcpTransportReadLine(char *buf, size_t bufsize)
{
    if (!fgets(buf, (int)bufsize, stdin))
        return feof(stdin) ? 0 : -1;

    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';

    return (int)len;
}

void mcpTransportWriteLine(const char *str, size_t len)
{
    fwrite(str, 1, len, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

/* --- HTTP/1.1 + SSE server (Streamable HTTP) --- */

#define MAX_POST_BODY 262144

static struct MHD_Daemon *g_daemon = NULL;

/* Per-connection state for POST body accumulation */
struct PostCtx {
    char *body;
    size_t len;
    size_t cap;
};

static void postCtxFree(struct PostCtx *pctx)
{
    if (!pctx) return;
    free(pctx->body);
    free(pctx);
}

static enum MHD_Result answerToConnection(
    void *cls,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *version,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls)
{
    (void)cls;
    (void)version;

    /* --- OPTIONS (CORS preflight) --- */
    if (strcmp(method, "OPTIONS") == 0) {
        struct MHD_Response *resp = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(resp, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        MHD_add_response_header(resp, "Access-Control-Allow-Headers", "Content-Type");
        MHD_add_response_header(resp, "Access-Control-Max-Age", "86400");
        enum MHD_Result rc = MHD_queue_response(connection, MHD_HTTP_OK, resp);
        MHD_destroy_response(resp);
        return rc;
    }

    /* --- GET /events — SSE endpoint (placeholder/connection info) --- */
    if (strcmp(method, "GET") == 0 && strcmp(url, "/events") == 0) {
        const char *body =
            "data: {\"endpoint\":\"/events\",\"protocol\":\"sse\"}\n\n";
        struct MHD_Response *resp = MHD_create_response_from_buffer(
            strlen(body), (void *)body, MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(resp, "Content-Type", "text/event-stream");
        MHD_add_response_header(resp, "Cache-Control", "no-cache");
        MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
        enum MHD_Result rc = MHD_queue_response(connection, MHD_HTTP_OK, resp);
        MHD_destroy_response(resp);
        /* Future: suspend connection + push monitor updates via SSE */
        return rc;
    }

    /* --- POST /mcp — JSON-RPC --- */
    if (strcmp(method, "POST") == 0 && strcmp(url, "/mcp") == 0) {
        if (*con_cls == NULL) {
            /* First call: allocate per-connection state */
            struct PostCtx *pctx = (struct PostCtx *)calloc(1, sizeof(struct PostCtx));
            pctx->cap = 4096;
            pctx->body = (char *)malloc(pctx->cap);
            pctx->body[0] = '\0';
            *con_cls = pctx;
            return MHD_YES;
        }

        if (*upload_data_size > 0) {
            /* Accumulate body */
            struct PostCtx *pctx = (struct PostCtx *)*con_cls;
            size_t needed = pctx->len + *upload_data_size + 1;
            if (needed > pctx->cap) {
                while (pctx->cap < needed) pctx->cap *= 2;
                if (pctx->cap > MAX_POST_BODY) {
                    postCtxFree(pctx);
                    *con_cls = NULL;
                    struct MHD_Response *resp = MHD_create_response_from_buffer(
                        0, NULL, MHD_RESPMEM_PERSISTENT);
                    enum MHD_Result rc = MHD_queue_response(
                        connection, MHD_HTTP_CONTENT_TOO_LARGE, resp);
                    MHD_destroy_response(resp);
                    return rc;
                }
                pctx->body = (char *)realloc(pctx->body, pctx->cap);
            }
            memcpy(pctx->body + pctx->len, upload_data, *upload_data_size);
            pctx->len += *upload_data_size;
            pctx->body[pctx->len] = '\0';
            *upload_data_size = 0;
            return MHD_YES;
        }

        /* Body complete — process JSON-RPC */
        struct PostCtx *pctx = (struct PostCtx *)*con_cls;
        size_t respLen = 0;
        char *respStr = mcpProtocolHandleRequestStr(
            pctx->body ? pctx->body : "", &respLen);

        postCtxFree(pctx);
        *con_cls = NULL;

        if (!respStr) {
            respStr = strdup("{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"}}");
            respLen = strlen(respStr);
        }

        struct MHD_Response *resp = MHD_create_response_from_buffer(
            respLen, respStr, MHD_RESPMEM_MUST_FREE);
        MHD_add_response_header(resp, "Content-Type", "application/json");
        MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(resp, "MCP-Protocol-Version", MCP_PROTOCOL_VERSION);
        enum MHD_Result rc = MHD_queue_response(connection, MHD_HTTP_OK, resp);
        MHD_destroy_response(resp);
        return rc;
    }

    /* --- 404 --- */
    struct MHD_Response *resp = MHD_create_response_from_buffer(
        0, NULL, MHD_RESPMEM_PERSISTENT);
    enum MHD_Result rc = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, resp);
    MHD_destroy_response(resp);
    return rc;
}

int mcpHttpServerStart(int port)
{
    if (g_daemon) {
        fprintf(stderr, "mcpServer: HTTP server already running\n");
        return -1;
    }

    g_daemon = MHD_start_daemon(
        MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD,
        port,
        NULL, NULL,
        &answerToConnection, NULL,
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)120,
        MHD_OPTION_END);

    if (!g_daemon) {
        fprintf(stderr, "mcpServer: failed to start HTTP server on port %d\n", port);
        return -1;
    }

    printf("mcpServer: listening on HTTP port %d (Streamable HTTP)\n", port);
    return 0;
}

void mcpHttpServerStop(void)
{
    if (!g_daemon) return;
    MHD_stop_daemon(g_daemon);
    g_daemon = NULL;
    printf("mcpServer: HTTP server stopped\n");
}

int mcpHttpServerIsRunning(void)
{
    return g_daemon != NULL;
}
