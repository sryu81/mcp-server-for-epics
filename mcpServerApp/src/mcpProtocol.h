#ifndef INC_mcpProtocol_H
#define INC_mcpProtocol_H

#include <stddef.h>

typedef void (*McpWriteFn)(void *ctx, const char *data, size_t len);

void mcpProtocolInit(void);
void mcpProtocolHandleRequest(const char *line, McpWriteFn writeFn, void *writeCtx);

/* Return response as allocated string. Caller must free(). */
char *mcpProtocolHandleRequestStr(const char *line, size_t *outLen);

#endif /* INC_mcpProtocol_H */
