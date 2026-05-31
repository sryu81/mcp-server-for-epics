#ifndef INC_mcpTransport_H
#define INC_mcpTransport_H

#include <stddef.h>

/* Stdio transport */
int mcpTransportInit(void);
int mcpTransportReadLine(char *buf, size_t bufsize);
void mcpTransportWriteLine(const char *str, size_t len);

/* HTTP/1.1 + SSE server (Streamable HTTP) */
int mcpHttpServerStart(int port);
void mcpHttpServerStop(void);
int mcpHttpServerIsRunning(void);

#endif /* INC_mcpTransport_H */
