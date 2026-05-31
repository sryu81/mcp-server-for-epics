#include <cstring>
#include "mcpTools.h"
#include "mcpJson.h"

static McpToolDef tools[] = {
    {
        "epics_get",
        "Read one or more EPICS PV values (pvAccess or Channel Access, default: pva)",
        toolGetHandler,
        toolGetSchema
    },
    {
        "epics_put",
        "Write a value to an EPICS PV (pvAccess or Channel Access, default: pva)",
        toolPutHandler,
        toolPutSchema
    },
    {
        "epics_monitor",
        "Monitor EPICS PVs for changes over a time period (pvAccess or CA)",
        toolMonitorHandler,
        toolMonitorSchema
    },
    {
        "epics_info",
        "Get connection and metadata info for EPICS PVs (pvAccess or CA)",
        toolInfoHandler,
        toolInfoSchema
    },
    {
        "epics_iocsh",
        "Execute an EPICS IOC shell command and return its output",
        toolIocshHandler,
        toolIocshSchema
    },
    {
        "epics_version",
        "Get EPICS Base version, build info, and enabled protocols",
        toolVersionHandler,
        toolVersionSchema
    },
    {
        "epics_status",
        "Get EPICS MCP server status including protocol availability",
        toolStatusHandler,
        toolStatusSchema
    },
    {
        "epics_dbl",
        "List all database records, optionally filtered by pattern",
        toolDblHandler,
        toolDblSchema
    },
    {
        "epics_dbload",
        "Load an EPICS database file with optional macro substitutions",
        toolDbloadHandler,
        toolDbloadSchema
    },
    { NULL, NULL, NULL, NULL }
};

void mcpToolsInit(void)
{
    /* Static tool table, no dynamic init needed */
}

const McpToolDef *mcpToolsFind(const char *name)
{
    McpToolDef *t;
    for (t = tools; t->name; t++) {
        if (strcmp(t->name, name) == 0)
            return t;
    }
    return NULL;
}

void mcpToolsWriteList(yajl_gen gen)
{
    McpToolDef *t;

    yajl_gen_array_open(gen);
    for (t = tools; t->name; t++) {
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "name", t->name);
        mcpGenKeyString(gen, "description", t->description);

        mcpGenString(gen, "inputSchema");
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "type", "object");

        mcpGenString(gen, "properties");
        yajl_gen_map_open(gen);
        t->writeSchema(gen);
        yajl_gen_map_close(gen);

        yajl_gen_map_close(gen);
        yajl_gen_map_close(gen);
    }
    yajl_gen_array_close(gen);
}
