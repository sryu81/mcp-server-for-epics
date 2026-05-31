#include <cstring>
#include "mcpTools.h"
#include "mcpJson.h"

/* Required-parameter lists (NULL-terminated) */
static const char * const s_req_get[]     = {"pvnames", NULL};
static const char * const s_req_put[]     = {"pvname", "value", NULL};
static const char * const s_req_monitor[] = {"pvnames", "duration", NULL};
static const char * const s_req_info[]    = {"pvnames", NULL};
static const char * const s_req_iocsh[]   = {"command", NULL};
static const char * const s_req_dbload[]  = {"filename", NULL};
static const char * const s_req_none[]    = {NULL}; /* optional params, none required */

static McpToolDef tools[] = {
    {
        "epics_get",
        "Read PV Values",
        "Read one or more EPICS PV values. Returns value, alarm status, and timestamp for each PV. Protocol: pva (default) or ca.",
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
        "Collect all value changes from EPICS PVs over a time window (duration: 1.0–30.0 seconds, clamped). Returns list of timestamped updates. Protocol: pva (default) or ca.",
        toolMonitorHandler, toolMonitorSchema, s_req_monitor
    },
    {
        "epics_info",
        "PV Connection Info",
        "Get connection and metadata for EPICS PVs. Returns type, host, alarm status, units, and display limits for each PV. Protocol: pva (default) or ca.",
        toolInfoHandler, toolInfoSchema, s_req_info
    },
    {
        "epics_iocsh",
        "IOC Shell Command",
        "[IOC-embedded only] Execute an EPICS IOC shell command and return its output. Use command 'dbl' to list all PV names.",
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
        "[IOC-embedded only] List database records. Always returns a stub — use epics_iocsh with command 'dbl' instead.",
        toolDblHandler, toolDblSchema, s_req_none
    },
    {
        "epics_dbload",
        "Load Database File",
        "[IOC-embedded only] Load an EPICS database file. Always returns a stub — use epics_iocsh with command 'dbLoadRecords(\"file.db\",\"P=X:,R=Y\")' instead.",
        toolDbloadHandler, toolDbloadSchema, s_req_dbload
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
