#ifndef INC_mcpTools_H
#define INC_mcpTools_H

#include "mcpJson.h"
#include <yajl_gen.h>

typedef int (*McpToolHandler)(const McpJsonValue *params, yajl_gen gen);

typedef struct {
    const char *name;
    const char *description;
    McpToolHandler handler;
    void (*writeSchema)(yajl_gen gen);
} McpToolDef;

void mcpToolsInit(void);
const McpToolDef *mcpToolsFind(const char *name);
void mcpToolsWriteList(yajl_gen gen);

/* Unified PV tools */
int toolGetHandler(const McpJsonValue *params, yajl_gen gen);
void toolGetSchema(yajl_gen gen);

int toolPutHandler(const McpJsonValue *params, yajl_gen gen);
void toolPutSchema(yajl_gen gen);

int toolMonitorHandler(const McpJsonValue *params, yajl_gen gen);
void toolMonitorSchema(yajl_gen gen);

int toolInfoHandler(const McpJsonValue *params, yajl_gen gen);
void toolInfoSchema(yajl_gen gen);

/* System tools */
int toolIocshHandler(const McpJsonValue *params, yajl_gen gen);
void toolIocshSchema(yajl_gen gen);

int toolVersionHandler(const McpJsonValue *params, yajl_gen gen);
void toolVersionSchema(yajl_gen gen);

int toolStatusHandler(const McpJsonValue *params, yajl_gen gen);
void toolStatusSchema(yajl_gen gen);

int toolDblHandler(const McpJsonValue *params, yajl_gen gen);
void toolDblSchema(yajl_gen gen);

int toolDbloadHandler(const McpJsonValue *params, yajl_gen gen);
void toolDbloadSchema(yajl_gen gen);

#endif /* INC_mcpTools_H */
