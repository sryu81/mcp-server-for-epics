#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <iocsh.h>

#include "mcpTools.h"
#include "mcpJson.h"
#include "mcpServer.h"

#define IOCSH_BUF_SIZE 8192

void toolIocshSchema(yajl_gen gen)
{
    mcpGenString(gen, "command");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "string");
    mcpGenKeyString(gen, "description", "IOC shell command string, e.g. 'dbl' (list PVs), 'dbgf PVname' (read record), 'dbpf PVname value' (write record)");
    yajl_gen_map_close(gen);
}

int toolIocshHandler(const McpJsonValue *params, yajl_gen gen)
{
    const char *command;

    if (!params) {
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "error", "params required");
        yajl_gen_map_close(gen);
        return -1;
    }

    command = mcpJsonGetString(mcpJsonMapGet(params, "command"));
    if (!command) {
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "error", "command string is required");
        yajl_gen_map_close(gen);
        return -1;
    }

#ifndef _WIN32
    /* Capture stdout by redirecting to a pipe */
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "error", "pipe creation failed");
        yajl_gen_map_close(gen);
        return -1;
    }

    fflush(stdout);
    int savedStdout = dup(STDOUT_FILENO);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    int rc = iocshCmd(command);

    fflush(stdout);
    dup2(savedStdout, STDOUT_FILENO);
    close(savedStdout);

    /* Read captured output */
    char outBuf[IOCSH_BUF_SIZE];
    ssize_t nRead = read(pipefd[0], outBuf, sizeof(outBuf) - 1);
    close(pipefd[0]);

    if (nRead < 0) nRead = 0;
    outBuf[nRead] = '\0';

    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "command", command);
    mcpGenKeyInt(gen, "status", rc);
    mcpGenKeyString(gen, "output", outBuf);
    yajl_gen_map_close(gen);
#else
    /* On Windows, just run without capture */
    int rc = iocshCmd(command);
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "command", command);
    mcpGenKeyInt(gen, "status", rc);
    mcpGenKeyString(gen, "output", "(output capture not available on Windows)");
    yajl_gen_map_close(gen);
#endif

    return rc;
}
