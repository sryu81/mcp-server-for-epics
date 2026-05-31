#include <cstring>
#include <vector>
#include <string>

#include "mcpTools.h"
#include "mcpJson.h"
#include "mcpServer.h"
#include "pvaOps.h"
#include "caOps.h"

void toolInfoSchema(yajl_gen gen)
{
    mcpGenString(gen, "pvnames");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "array");
    mcpGenString(gen, "items");
    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "type", "string");
    yajl_gen_map_close(gen);
    mcpGenKeyString(gen, "description", "PV name(s) to query");
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

int toolInfoHandler(const McpJsonValue *params, yajl_gen gen)
{
    McpJsonValue *pvnamesVal = params ? mcpJsonMapGet(params, "pvnames") : NULL;
    if (!pvnamesVal || pvnamesVal->type != MCP_JSON_ARRAY ||
        mcpJsonArrayCount(pvnamesVal) == 0) {
        yajl_gen_map_open(gen);
        mcpGenKeyString(gen, "error", "pvnames array is required");
        yajl_gen_map_close(gen);
        return -1;
    }

    const char *protocol = mcpGetProtocol(params);
    double timeout = MCP_DEFAULT_TIMEOUT;

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
        result = caOps::info(pvnames, timeout);
    } else {
        if (!mcpState.pvaEnabled) {
            yajl_gen_map_open(gen);
            mcpGenKeyString(gen, "error", "pvAccess is disabled (--no-pva)");
            yajl_gen_map_close(gen);
            return -1;
        }
        result = pvaOps::info(pvnames, timeout);
    }

    yajl_gen_string(gen, (const unsigned char *)result.c_str(), result.size());
    return 0;
}
