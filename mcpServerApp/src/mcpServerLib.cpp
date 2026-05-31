
#include <cstdio>
#include <cstring>
#include <csignal>

#include <cadef.h>

#include "mcpServerLib.h"
#include "mcpServer.h"
#include "mcpTransport.h"
#include "mcpProtocol.h"
#include "pvaOps.h"
#include "caOps.h"

McpServerState mcpState = {0, 0, 1, 1};

const char *mcpGetProtocol(const McpJsonValue *params)
{
    if (!params) return "pva";
    const char *p = mcpJsonGetString(mcpJsonMapGet(params, "protocol"));
    if (p && strcmp(p, "ca") == 0) return "ca";
    return "pva";
}

/* --- HTTP mode (IOC-embedded) --- */

int mcpServerStart(int port)
{
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "mcpServer: invalid port %d\n", port);
        return -1;
    }

    mcpProtocolInit();

    if (mcpState.caEnabled)
        caOps::init();
    if (mcpState.pvaEnabled)
        pvaOps::init();

    return mcpHttpServerStart(port);
}

void mcpServerStop(void)
{
    mcpHttpServerStop();

    if (mcpState.pvaEnabled)
        pvaOps::shutdown();
    if (mcpState.caEnabled)
        caOps::shutdown();
}

int mcpServerIsRunning(void)
{
    return mcpHttpServerIsRunning();
}

/* --- Stdio mode (standalone executable) --- */

static void stdioWriteFn(void *ctx, const char *data, size_t len)
{
    (void)ctx;
    mcpTransportWriteLine(data, len);
}

static void signalHandler(int sig)
{
    (void)sig;
    mcpState.shutdownRequested = 1;
}

int mcpServerRunStdio(int argc, char *argv[])
{
    char line[MCP_MAX_LINE];

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-ca") == 0)
            mcpState.caEnabled = 0;
        else if (strcmp(argv[i], "--no-pva") == 0)
            mcpState.pvaEnabled = 0;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, "Usage: epicsMcpServer [--no-ca] [--no-pva]\n");
            fprintf(stderr, "  --no-ca   Disable Channel Access protocol\n");
            fprintf(stderr, "  --no-pva  Disable pvAccess protocol\n");
            return 0;
        }
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    mcpTransportInit();
    mcpProtocolInit();

    if (mcpState.caEnabled)
        caOps::init();
    if (mcpState.pvaEnabled)
        pvaOps::init();

    while (!mcpState.shutdownRequested) {
        int n = mcpTransportReadLine(line, sizeof(line));
        if (n <= 0) break;
        mcpProtocolHandleRequest(line, stdioWriteFn, NULL);
    }

    if (mcpState.pvaEnabled)
        pvaOps::shutdown();
    if (mcpState.caEnabled)
        caOps::shutdown();

    return 0;
}
