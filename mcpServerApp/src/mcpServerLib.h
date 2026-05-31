#ifndef INC_mcpServerLib_H
#define INC_mcpServerLib_H

#include "pv/mcpServerAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

MCPSERVER_API int mcpServerStart(int port);
MCPSERVER_API void mcpServerStop(void);
MCPSERVER_API int mcpServerRunStdio(int argc, char *argv[]);
MCPSERVER_API int mcpServerIsRunning(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_mcpServerLib_H */
