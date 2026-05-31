#include <cstring>
#include <string>
#include <sstream>

#include "mcpTools.h"
#include "mcpJson.h"
#include "mcpServer.h"
#include "pvaOps.h"
#include "caOps.h"

void toolPutSchema(yajl_gen gen)
{
    mcpGenString(gen, "pvname");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "string");
    mcpGenKeyString(gen, "description", "PV name to write");
    yajl_gen_map_close(gen);

    mcpGenString(gen, "value");
    yajl_gen_map_open(gen);
    mcpGenString(gen, "type");
    yajl_gen_array_open(gen);
    mcpGenString(gen, "string");
    mcpGenString(gen, "number");
    mcpGenString(gen, "integer");
    yajl_gen_array_close(gen);
    mcpGenKeyString(gen, "description", "Value to write (string, integer, or double; no arrays/objects)");
    yajl_gen_map_close(gen);

    mcpGenString(gen, "protocol");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "string");
    mcpGenKeyString(gen, "description", "Protocol: 'pva' (default) or 'ca'");
    yajl_gen_map_close(gen);

    mcpGenString(gen, "timeout");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "number");
    mcpGenKeyString(gen, "description", "Timeout in seconds (default: 5.0)");
    yajl_gen_map_close(gen);
}

int toolPutHandler(const McpJsonValue *params, yajl_gen gen)
{
    if (!params) {
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "error", "params required");
        yajl_gen_map_close(gen);
        return -1;
    }

    const char *pvname = mcpJsonGetString(mcpJsonMapGet(params, "pvname"));
    McpJsonValue *valueVal = mcpJsonMapGet(params, "value");
    if (!pvname || !valueVal) {
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "error", "pvname and value are required");
        yajl_gen_map_close(gen);
        return -1;
    }

    /* Convert value to string representation */
    std::string valueStr;
    if (valueVal->type == MCP_JSON_STRING) {
        valueStr = mcpJsonGetString(valueVal);
    } else if (valueVal->type == MCP_JSON_INT) {
        std::ostringstream oss;
        oss << mcpJsonGetInt(valueVal);
        valueStr = oss.str();
    } else if (valueVal->type == MCP_JSON_DOUBLE) {
        std::ostringstream oss;
        oss << mcpJsonGetDouble(valueVal);
        valueStr = oss.str();
    } else {
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "error", "unsupported value type");
        yajl_gen_map_close(gen);
        return -1;
    }

    const char *protocol = mcpGetProtocol(params);
    double timeout = MCP_DEFAULT_TIMEOUT;
    McpJsonValue *tv = mcpJsonMapGet(params, "timeout");
    if (tv) timeout = mcpJsonGetDouble(tv);

    std::string result;
    if (strcmp(protocol, "ca") == 0) {
        if (!mcpState.caEnabled) {
            yajl_gen_map_open(gen);
            mcpGenKeyString(gen, "error", "Channel Access is disabled (--no-ca)");
            yajl_gen_map_close(gen);
            return -1;
        }
        result = caOps::put(pvname, valueStr, timeout);
    } else {
        if (!mcpState.pvaEnabled) {
            yajl_gen_map_open(gen);
            mcpGenKeyString(gen, "error", "pvAccess is disabled (--no-pva)");
            yajl_gen_map_close(gen);
            return -1;
        }
        result = pvaOps::put(pvname, valueStr, timeout);
    }

    yajl_gen_string(gen, (const unsigned char *)result.c_str(), result.size());
    return 0;
}
