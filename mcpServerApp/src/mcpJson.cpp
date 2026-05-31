#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <yajl_parse.h>
#include <yajl_gen.h>

#include "mcpJson.h"
#include "mcpTransport.h"

/* --- DOM builder on top of yajl SAX --- */

#define STACK_MAX 64

typedef struct {
    McpJsonValue *stack[STACK_MAX];
    char *pendingKey[STACK_MAX];
    int depth;
    McpJsonValue *root;
    int error;
} ParseCtx;

static McpJsonValue *jsonNewValue(McpJsonType type)
{
    McpJsonValue *v = (McpJsonValue *)calloc(1, sizeof(McpJsonValue));
    if (v) v->type = type;
    return v;
}

static void pushValue(ParseCtx *ctx, McpJsonValue *val)
{
    if (ctx->depth <= 0) {
        ctx->root = val;
        return;
    }

    McpJsonValue *parent = ctx->stack[ctx->depth - 1];
    if (parent->type == MCP_JSON_ARRAY) {
        size_t cnt = parent->u.arrayVal.count;
        if (cnt >= parent->u.arrayVal.capacity) {
            size_t newCap = parent->u.arrayVal.capacity ? parent->u.arrayVal.capacity * 2 : 8;
            parent->u.arrayVal.items = (McpJsonValue **)realloc(parent->u.arrayVal.items,
                                               newCap * sizeof(McpJsonValue *));
            parent->u.arrayVal.capacity = newCap;
        }
        parent->u.arrayVal.items[cnt] = val;
        parent->u.arrayVal.count = cnt + 1;
    }
    else if (parent->type == MCP_JSON_MAP) {
        size_t cnt = parent->u.mapVal.count;
        if (cnt >= parent->u.mapVal.capacity) {
            size_t newCap = parent->u.mapVal.capacity ? parent->u.mapVal.capacity * 2 : 8;
            parent->u.mapVal.keys = (char **)realloc(parent->u.mapVal.keys,
                                            newCap * sizeof(char *));
            parent->u.mapVal.values = (McpJsonValue **)realloc(parent->u.mapVal.values,
                                              newCap * sizeof(McpJsonValue *));
            parent->u.mapVal.capacity = newCap;
        }
        parent->u.mapVal.keys[cnt] = ctx->pendingKey[ctx->depth - 1];
        parent->u.mapVal.values[cnt] = val;
        parent->u.mapVal.count = cnt + 1;
        ctx->pendingKey[ctx->depth - 1] = NULL;
    }
}

static int cb_null(void *c)
{
    ParseCtx *ctx = static_cast<ParseCtx*>(c);
    pushValue(ctx, jsonNewValue(MCP_JSON_NULL));
    return 1;
}

static int cb_boolean(void *c, int boolVal)
{
    ParseCtx *ctx = static_cast<ParseCtx*>(c);
    McpJsonValue *v = jsonNewValue(MCP_JSON_BOOL);
    v->u.boolVal = boolVal;
    pushValue(ctx, v);
    return 1;
}

static int cb_integer(void *c, long long intVal)
{
    ParseCtx *ctx = static_cast<ParseCtx*>(c);
    McpJsonValue *v = jsonNewValue(MCP_JSON_INT);
    v->u.intVal = intVal;
    pushValue(ctx, v);
    return 1;
}

static int cb_double(void *c, double doubleVal)
{
    ParseCtx *ctx = static_cast<ParseCtx*>(c);
    McpJsonValue *v = jsonNewValue(MCP_JSON_DOUBLE);
    v->u.doubleVal = doubleVal;
    pushValue(ctx, v);
    return 1;
}

static int cb_string(void *c, const unsigned char *str, size_t len)
{
    ParseCtx *ctx = static_cast<ParseCtx*>(c);
    McpJsonValue *v = jsonNewValue(MCP_JSON_STRING);
    v->u.stringVal.str = (char *)malloc(len + 1);
    memcpy(v->u.stringVal.str, str, len);
    v->u.stringVal.str[len] = '\0';
    v->u.stringVal.len = len;
    pushValue(ctx, v);
    return 1;
}

static int cb_map_key(void *c, const unsigned char *key, size_t len)
{
    ParseCtx *ctx = static_cast<ParseCtx*>(c);
    if (ctx->depth <= 0) return 0;
    free(ctx->pendingKey[ctx->depth - 1]);
    ctx->pendingKey[ctx->depth - 1] = (char *)malloc(len + 1);
    memcpy(ctx->pendingKey[ctx->depth - 1], key, len);
    ctx->pendingKey[ctx->depth - 1][len] = '\0';
    return 1;
}

static int cb_start_map(void *c)
{
    ParseCtx *ctx = static_cast<ParseCtx*>(c);
    McpJsonValue *v = jsonNewValue(MCP_JSON_MAP);
    pushValue(ctx, v);
    if (ctx->depth >= STACK_MAX) { ctx->error = 1; return 0; }
    ctx->stack[ctx->depth] = v;
    ctx->pendingKey[ctx->depth] = NULL;
    ctx->depth++;
    return 1;
}

static int cb_end_map(void *c)
{
    ParseCtx *ctx = static_cast<ParseCtx*>(c);
    if (ctx->depth > 0) ctx->depth--;
    return 1;
}

static int cb_start_array(void *c)
{
    ParseCtx *ctx = static_cast<ParseCtx*>(c);
    McpJsonValue *v = jsonNewValue(MCP_JSON_ARRAY);
    pushValue(ctx, v);
    if (ctx->depth >= STACK_MAX) { ctx->error = 1; return 0; }
    ctx->stack[ctx->depth] = v;
    ctx->pendingKey[ctx->depth] = NULL;
    ctx->depth++;
    return 1;
}

static int cb_end_array(void *c)
{
    ParseCtx *ctx = static_cast<ParseCtx*>(c);
    if (ctx->depth > 0) ctx->depth--;
    return 1;
}

static const yajl_callbacks parseCallbacks = {
    cb_null,
    cb_boolean,
    cb_integer,
    cb_double,
    NULL, /* yajl_number — not used, let yajl dispatch to integer/double */
    cb_string,
    cb_start_map,
    cb_map_key,
    cb_end_map,
    cb_start_array,
    cb_end_array
};

McpJsonValue *mcpJsonParse(const char *json, size_t len)
{
    ParseCtx ctx;
    memset(&ctx, 0, sizeof(ctx));

    yajl_handle yh = yajl_alloc(&parseCallbacks, NULL, &ctx);
    if (!yh) return NULL;

    yajl_config(yh, yajl_allow_json5, 0);

    yajl_status ys = yajl_parse(yh, (const unsigned char *)json, len);
    if (ys == yajl_status_ok)
        ys = yajl_complete_parse(yh);

    yajl_free(yh);

    if (ys != yajl_status_ok || ctx.error) {
        mcpJsonFree(ctx.root);
        return NULL;
    }

    return ctx.root;
}

void mcpJsonFree(McpJsonValue *val)
{
    size_t i;
    if (!val) return;

    switch (val->type) {
    case MCP_JSON_STRING:
        free(val->u.stringVal.str);
        break;
    case MCP_JSON_ARRAY:
        for (i = 0; i < val->u.arrayVal.count; i++)
            mcpJsonFree(val->u.arrayVal.items[i]);
        free(val->u.arrayVal.items);
        break;
    case MCP_JSON_MAP:
        for (i = 0; i < val->u.mapVal.count; i++) {
            free(val->u.mapVal.keys[i]);
            mcpJsonFree(val->u.mapVal.values[i]);
        }
        free(val->u.mapVal.keys);
        free(val->u.mapVal.values);
        break;
    default:
        break;
    }
    free(val);
}

McpJsonValue *mcpJsonMapGet(const McpJsonValue *map, const char *key)
{
    size_t i;
    if (!map || map->type != MCP_JSON_MAP) return NULL;
    for (i = 0; i < map->u.mapVal.count; i++) {
        if (strcmp(map->u.mapVal.keys[i], key) == 0)
            return map->u.mapVal.values[i];
    }
    return NULL;
}

const char *mcpJsonGetString(const McpJsonValue *val)
{
    if (!val || val->type != MCP_JSON_STRING) return NULL;
    return val->u.stringVal.str;
}

long long mcpJsonGetInt(const McpJsonValue *val)
{
    if (!val) return 0;
    if (val->type == MCP_JSON_INT) return val->u.intVal;
    if (val->type == MCP_JSON_DOUBLE) return (long long)val->u.doubleVal;
    return 0;
}

double mcpJsonGetDouble(const McpJsonValue *val)
{
    if (!val) return 0.0;
    if (val->type == MCP_JSON_DOUBLE) return val->u.doubleVal;
    if (val->type == MCP_JSON_INT) return (double)val->u.intVal;
    return 0.0;
}

int mcpJsonGetBool(const McpJsonValue *val)
{
    if (!val || val->type != MCP_JSON_BOOL) return 0;
    return val->u.boolVal;
}

size_t mcpJsonArrayCount(const McpJsonValue *val)
{
    if (!val || val->type != MCP_JSON_ARRAY) return 0;
    return val->u.arrayVal.count;
}

McpJsonValue *mcpJsonArrayGet(const McpJsonValue *val, size_t index)
{
    if (!val || val->type != MCP_JSON_ARRAY) return NULL;
    if (index >= val->u.arrayVal.count) return NULL;
    return val->u.arrayVal.items[index];
}

/* --- JSON generation helpers --- */

void mcpGenString(yajl_gen gen, const char *str)
{
    yajl_gen_string(gen, (const unsigned char *)str, strlen(str));
}

void mcpGenKeyString(yajl_gen gen, const char *key, const char *val)
{
    mcpGenString(gen, key);
    mcpGenString(gen, val);
}

void mcpGenKeyInt(yajl_gen gen, const char *key, long long val)
{
    mcpGenString(gen, key);
    yajl_gen_integer(gen, val);
}

void mcpGenKeyDouble(yajl_gen gen, const char *key, double val)
{
    mcpGenString(gen, key);
    yajl_gen_double(gen, val);
}

void mcpGenKeyBool(yajl_gen gen, const char *key, int val)
{
    mcpGenString(gen, key);
    yajl_gen_bool(gen, val);
}

void mcpJsonFlushAndFree(yajl_gen gen)
{
    const unsigned char *buf;
    size_t len;
    yajl_gen_get_buf(gen, &buf, &len);
    mcpTransportWriteLine((const char *)buf, len);
    yajl_gen_free(gen);
}

void mcpJsonSendError(long long id, int code, const char *message)
{
    yajl_gen gen = yajl_gen_alloc(NULL);

    yajl_gen_map_open(gen);
    mcpGenKeyString(gen, "jsonrpc", "2.0");
    mcpGenString(gen, "id");
    yajl_gen_integer(gen, id);
    mcpGenString(gen, "error");
    yajl_gen_map_open(gen);
    mcpGenKeyInt(gen, "code", code);
    mcpGenKeyString(gen, "message", message);
    yajl_gen_map_close(gen);
    yajl_gen_map_close(gen);

    mcpJsonFlushAndFree(gen);
}
