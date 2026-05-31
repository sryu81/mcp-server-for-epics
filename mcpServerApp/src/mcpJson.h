#ifndef INC_mcpJson_H
#define INC_mcpJson_H

#include <stddef.h>

typedef enum {
    MCP_JSON_NULL,
    MCP_JSON_BOOL,
    MCP_JSON_INT,
    MCP_JSON_DOUBLE,
    MCP_JSON_STRING,
    MCP_JSON_ARRAY,
    MCP_JSON_MAP
} McpJsonType;

typedef struct McpJsonValue {
    McpJsonType type;
    union {
        int boolVal;
        long long intVal;
        double doubleVal;
        struct { char *str; size_t len; } stringVal;
        struct { struct McpJsonValue **items; size_t count; size_t capacity; } arrayVal;
        struct {
            char **keys;
            struct McpJsonValue **values;
            size_t count;
            size_t capacity;
        } mapVal;
    } u;
} McpJsonValue;

/* Parse JSON string into DOM tree. Caller must free with mcpJsonFree(). */
McpJsonValue *mcpJsonParse(const char *json, size_t len);

/* Free a parsed JSON value tree */
void mcpJsonFree(McpJsonValue *val);

/* Lookup key in a map value. Returns NULL if not found or not a map. */
McpJsonValue *mcpJsonMapGet(const McpJsonValue *map, const char *key);

/* Get string from a JSON string value. Returns NULL if not a string. */
const char *mcpJsonGetString(const McpJsonValue *val);

/* Get integer from a JSON int value. Returns 0 if not an int. */
long long mcpJsonGetInt(const McpJsonValue *val);

/* Get double from a JSON number value. Handles int→double. */
double mcpJsonGetDouble(const McpJsonValue *val);

/* Get bool from a JSON bool value. Returns 0 if not a bool. */
int mcpJsonGetBool(const McpJsonValue *val);

/* Get array count. Returns 0 if not an array. */
size_t mcpJsonArrayCount(const McpJsonValue *val);

/* Get array item by index. Returns NULL if out of bounds. */
McpJsonValue *mcpJsonArrayGet(const McpJsonValue *val, size_t index);

/* --- JSON generation helpers --- */

#include <yajl_gen.h>

/* Helper: write a C string as a yajl string */
void mcpGenString(yajl_gen gen, const char *str);

/* Helper: write a key-value pair where value is a string */
void mcpGenKeyString(yajl_gen gen, const char *key, const char *val);

/* Helper: write a key-value pair where value is an integer */
void mcpGenKeyInt(yajl_gen gen, const char *key, long long val);

/* Helper: write a key-value pair where value is a double */
void mcpGenKeyDouble(yajl_gen gen, const char *key, double val);

/* Helper: write a key-value pair where value is a bool */
void mcpGenKeyBool(yajl_gen gen, const char *key, int val);

/* Send a complete JSON-RPC error response to stdout */
void mcpJsonSendError(long long id, int code, const char *message);

/* Flush yajl_gen buffer to stdout as one line, then free gen */
void mcpJsonFlushAndFree(yajl_gen gen);

#endif /* INC_mcpJson_H */
