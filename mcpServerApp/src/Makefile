#*************************************************************************
# Copyright (c) 2024 EPICS Contributors
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ../..
include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -DMCPSERVER_API_BUILDING

# Shared library: libmcpServer.so
LIBRARY_IOC = mcpServer

mcpServer_SRCS = mcpJson.cpp mcpTransport.cpp mcpProtocol.cpp
mcpServer_SRCS += mcpTools.cpp mcpServerLib.cpp mcpServerRegister.cpp
mcpServer_SRCS += toolGet.cpp toolPut.cpp toolMonitor.cpp toolInfo.cpp
mcpServer_SRCS += toolIocsh.cpp toolSystem.cpp toolDbload.cpp
mcpServer_SRCS += pvaOps.cpp caOps.cpp
mcpServer_LIBS = pvaClient nt pvAccess pvAccessCA pvData ca Com
mcpServer_SYS_LIBS += microhttpd

# Installed public header
INC += pv/mcpServerAPI.h

# DBD for IOC integration
DBD += mcpServer.dbd

# Standalone executable (thin wrapper linking the library)
PROD_CMD = epicsMcpServer
epicsMcpServer_SRCS = mcpServerMain.cpp
epicsMcpServer_LIBS = mcpServer pvaClient nt pvAccess pvAccessCA pvData ca Com
epicsMcpServer_SYS_LIBS += microhttpd

include $(TOP)/configure/RULES
