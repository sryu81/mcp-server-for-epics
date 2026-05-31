#include <cstring>
#include "mcpTools.h"
#include "mcpJson.h"

/* Required-parameter lists (NULL-terminated) */
static const char * const s_req_get[]     = {"pvnames", NULL};
static const char * const s_req_put[]     = {"pvname", "value", NULL};
static const char * const s_req_monitor[] = {"pvnames", "duration", NULL};
static const char * const s_req_info[]    = {"pvnames", NULL};
static const char * const s_req_iocsh[]   = {"command", NULL};
static const char * const s_req_none[]    = {NULL}; /* optional params, none required */

static McpToolDef tools[] = {
    {
        "epics_get",
        "Read PV Values",
        "Read one or more EPICS PV values (pvAccess or Channel Access, default: pva)",
        toolGetHandler, toolGetSchema, s_req_get
    },
    {
        "epics_put",
        "Write PV Value",
        "Write a value to an EPICS PV (pvAccess or Channel Access, default: pva)",
        toolPutHandler, toolPutSchema, s_req_put
    },
    {
        "epics_monitor",
        "Monitor PVs",
        "Monitor EPICS PVs for changes over a time period (pvAccess or CA)",
        toolMonitorHandler, toolMonitorSchema, s_req_monitor
    },
    {
        "epics_info",
        "PV Connection Info",
        "Get connection and metadata info for EPICS PVs (pvAccess or CA)",
        toolInfoHandler, toolInfoSchema, s_req_info
    },
    {
        "epics_iocsh",
        "IOC Shell Command",
        "Execute an EPICS IOC shell command and return its output",
        toolIocshHandler, toolIocshSchema, s_req_iocsh
    },
    {
        "epics_version",
        "EPICS Version",
        "Get EPICS Base version, build info, and enabled protocols",
        toolVersionHandler, toolVersionSchema, NULL  /* no params */
    },
    {
        "epics_status",
        "Server Status",
        "Get EPICS MCP server status including protocol availability",
        toolStatusHandler, toolStatusSchema, NULL  /* no params */
    },
    {
        "epics_dbl",
        "List Database Records",
        "List all database records, optionally filtered by pattern",
        toolDblHandler, toolDblSchema, s_req_none
    },
    {
        "epics_dbload",
        "Load Database File",
        "Load an EPICS database file with optional macro substitutions",
        toolDbloadHandler, toolDbloadSchema, s_req_none
    },
    { NULL, NULL, NULL, NULL, NULL, NULL }
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
        if (t->title)
            mcpGenKeyString(gen, "title", t->title);
        mcpGenKeyString(gen, "description", t->description);

        mcpGenString(gen, "inputSchema");
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "type", "object");

        if (t->required == NULL) {
            /* no-parameter tool: explicitly reject extra properties */
            mcpGenKeyBool(gen, "additionalProperties", 0);
        } else {
            mcpGenString(gen, "properties");
            yajl_gen_map_open(gen);
            t->writeSchema(gen);
            yajl_gen_map_close(gen);

            if (t->required[0]) {
                mcpGenString(gen, "required");
                yajl_gen_array_open(gen);
                for (const char * const *r = t->required; *r; r++)
                    mcpGenString(gen, *r);
                yajl_gen_array_close(gen);
            }
        }

        yajl_gen_map_close(gen); /* inputSchema */
        yajl_gen_map_close(gen); /* tool object */
    }
    yajl_gen_array_close(gen);
}
