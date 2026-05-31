#include <iocsh.h>

#include "mcpServerLib.h"

#include <epicsExport.h>

static const iocshArg startArg0 = {"port", iocshArgInt};
static const iocshArg *startArgs[] = {&startArg0};
static const iocshFuncDef startDef = {
    "mcpServerStart", 1, startArgs,
    "Start MCP server on specified TCP port"
};

static void startCall(const iocshArgBuf *args)
{
    mcpServerStart(args[0].ival);
}

static const iocshFuncDef stopDef = {
    "mcpServerStop", 0, NULL,
    "Stop MCP server"
};

static void stopCall(const iocshArgBuf *args)
{
    (void)args;
    mcpServerStop();
}

static void mcpServerRegistrar(void)
{
    iocshRegister(&startDef, startCall);
    iocshRegister(&stopDef, stopCall);
}

extern "C" {
    epicsExportRegistrar(mcpServerRegistrar);
}
