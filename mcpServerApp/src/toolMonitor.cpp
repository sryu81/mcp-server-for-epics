#include <cstring>
#include <vector>
#include <string>

#include "mcpTools.h"
#include "mcpJson.h"
#include "mcpServer.h"
#include "pvaOps.h"
#include "caOps.h"

void toolMonitorSchema(yajl_gen gen)
{
    mcpGenString(gen, "pvnames");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "array");
    mcpGenString(gen, "items");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "string");
    yajl_gen_map_close(gen);
    mcpGenKeyString(gen, "description", "PV name(s) to monitor");
    yajl_gen_map_close(gen);

    mcpGenString(gen, "duration");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "number");
    mcpGenKeyString(gen, "description", "Duration in seconds to monitor (minimum 1.0, maximum 30.0; values outside range are clamped)");
    yajl_gen_map_close(gen);

    mcpGenString(gen, "protocol");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "string");
    mcpGenKeyString(gen, "description", "Protocol: 'pva' (default) or 'ca'");
    yajl_gen_map_close(gen);
}

int toolMonitorHandler(const McpJsonValue *params, yajl_gen gen)
{
    McpJsonValue *pvnamesVal = params ? mcpJsonMapGet(params, "pvnames") : NULL;
    McpJsonValue *durationVal = params ? mcpJsonMapGet(params, "duration") : NULL;

    if (!pvnamesVal || pvnamesVal->type != MCP_JSON_ARRAY ||
        mcpJsonArrayCount(pvnamesVal) == 0 || !durationVal) {
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "error", "pvnames array and duration are required");
        yajl_gen_map_close(gen);
        return -1;
    }

    double duration = mcpJsonGetDouble(durationVal);
    if (duration <= 0) duration = 1.0;
    if (duration > MCP_MONITOR_MAX_DURATION) duration = MCP_MONITOR_MAX_DURATION;

    const char *protocol = mcpGetProtocol(params);

    std::vector<std::string> pvnames;
    for (size_t i = 0; i < mcpJsonArrayCount(pvnamesVal); i++) {
        const char *s = mcpJsonGetString(mcpJsonArrayGet(pvnamesVal, i));
        if (s) pvnames.push_back(s);
    }

    std::string result;
    if (strcmp(protocol, "ca") == 0) {
        if (!mcpState.caEnabled) {
            yajl_gen_map_open(gen);
            mcpGenKeyString(gen, "error", "Channel Access is disabled (--no-ca)");
            yajl_gen_map_close(gen);
            return -1;
        }
        result = caOps::monitor(pvnames, duration);
    } else {
        if (!mcpState.pvaEnabled) {
            yajl_gen_map_open(gen);
            mcpGenKeyString(gen, "error", "pvAccess is disabled (--no-pva)");
            yajl_gen_map_close(gen);
            return -1;
        }
        result = pvaOps::monitor(pvnames, duration);
    }

    yajl_gen_string(gen, (const unsigned char *)result.c_str(), result.size());
    return 0;
}
