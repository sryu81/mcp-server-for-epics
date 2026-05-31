#include <stdio.h>
#include <string.h>

#include <epicsVersion.h>
#include <cadef.h>

#include "mcpTools.h"
#include "mcpJson.h"
#include "mcpServer.h"

/* epics_version tool */

void toolVersionSchema(yajl_gen gen)
{
    /* No input parameters */
    (void)gen;
}

int toolVersionHandler(const McpJsonValue *params, yajl_gen gen)
{
    (void)params;

    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "version", EPICS_VERSION_STRING);

#ifdef EPICS_HOST_ARCH
    mcpGenKeyString(gen, "arch", EPICS_HOST_ARCH);
#endif

    mcpGenKeyInt(gen, "majorVersion", EPICS_VERSION);
    mcpGenKeyInt(gen, "minorVersion", EPICS_REVISION);
    mcpGenKeyInt(gen, "patchLevel", EPICS_MODIFICATION);
    mcpGenKeyBool(gen, "caEnabled", mcpState.caEnabled);
    mcpGenKeyBool(gen, "pvaEnabled", mcpState.pvaEnabled);
    yajl_gen_map_close(gen);

    return 0;
}

/* epics_status tool */

void toolStatusSchema(yajl_gen gen)
{
    /* No input parameters */
    (void)gen;
}

int toolStatusHandler(const McpJsonValue *params, yajl_gen gen)
{
    (void)params;

    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "serverName", MCP_SERVER_NAME);
    mcpGenKeyString(gen, "serverVersion", MCP_SERVER_VERSION);
    mcpGenKeyString(gen, "epicsVersion", EPICS_VERSION_STRING);

    mcpGenString(gen, "protocols");
    yajl_gen_map_open(gen);
    mcpGenKeyBool(gen, "pva", mcpState.pvaEnabled);
    mcpGenKeyBool(gen, "ca", mcpState.caEnabled);
    yajl_gen_map_close(gen);

    yajl_gen_map_close(gen);

    return 0;
}

/* epics_dbl tool */

void toolDblSchema(yajl_gen gen)
{
    mcpGenString(gen, "pattern");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "string");
    mcpGenKeyString(gen, "description",
                    "Optional record name pattern filter");
    yajl_gen_map_close(gen);
}

int toolDblHandler(const McpJsonValue *params, yajl_gen gen)
{
    (void)params;

    /*
     * dbl requires dbBase from database module. Since epicsMcpServer
     * links only against ca and Com (not dbCore), use iocsh to run dbl
     * and capture output. For now, return a stub response indicating
     * dbl is available via epics_iocsh tool.
     */
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "info",
        "Use epics_iocsh with command 'dbl' to list records. "
        "Direct dbl requires IOC database linkage.");
    yajl_gen_map_close(gen);

    return 0;
}
