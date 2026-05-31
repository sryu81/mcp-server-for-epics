
#include <stdio.h>
#include <string.h>

#include "mcpTools.h"
#include "mcpJson.h"
#include "mcpServer.h"

void toolDbloadSchema(yajl_gen gen)
{
    mcpGenString(gen, "filename");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "string");
    mcpGenKeyString(gen, "description", "Path to .db or .dbd file");
    yajl_gen_map_close(gen);

    mcpGenString(gen, "macros");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "string");
    mcpGenKeyString(gen, "description",
                    "Macro substitutions (e.g. 'P=TEST:,R=ai1')");
    yajl_gen_map_close(gen);
}

int toolDbloadHandler(const McpJsonValue *params, yajl_gen gen)
{
    (void)params;

    /*
     * dbLoadRecords/dbLoadDatabase require dbCore library.
     * Since epicsMcpServer links only ca + Com, database loading
     * should be done via epics_iocsh tool with commands like:
     *   dbLoadRecords("filename.db", "macros")
     */
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "info",
        "epics_dbload is a stub. Call epics_iocsh with {\"command\":\"dbLoadRecords(\\\"file.db\\\",\\\"P=X:,R=Y\\\")\"} to load database files.");
    yajl_gen_map_close(gen);

    return 0;
}
