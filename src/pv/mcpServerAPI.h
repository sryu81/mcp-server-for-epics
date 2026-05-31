#ifndef PV_MCPSERVER_API_H
#define PV_MCPSERVER_API_H

#if defined(MCPSERVER_API_BUILDING) && defined(epicsExportSharedSymbols)
#  error "Conflicting shared symbol macros"
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(MCPSERVER_API_BUILDING) && defined(EPICS_BUILD_DLL)
#    define MCPSERVER_API __declspec(dllexport)
#  elif !defined(MCPSERVER_API_BUILDING) && defined(EPICS_CALL_DLL)
#    define MCPSERVER_API __declspec(dllimport)
#  endif
#elif __GNUC__ >= 4
#  define MCPSERVER_API __attribute__((visibility("default")))
#endif

#ifndef MCPSERVER_API
#  define MCPSERVER_API
#endif

#endif /* PV_MCPSERVER_API_H */
