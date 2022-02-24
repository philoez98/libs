#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

typedef struct
{
    HANDLE DumpFile;
    EXCEPTION_POINTERS* ExceptionData;
} exception_payload;

typedef void exception_callback(exception_payload ExceptionPayload);

void InitializeCrashHandler(const char* DumpPath, exception_callback* ExceptionCallback);


#ifdef CRASH_HANDLER_IMPLEMENTATION

#include <DbgHelp.h>
#include <string.h>

#pragma comment(lib, "dbghelp.lib")

typedef struct
{
    char DumpFilePath[MAX_PATH];
    exception_callback* ExceptionCallback;
} crash_context;

static crash_context CrashContext;

static HANDLE CreateMiniDump(const char* DumpPath, EXCEPTION_POINTERS* ExceptionData)
{
    HANDLE DumpFile = CreateFile(DumpPath, GENERIC_WRITE, FILE_SHARE_READ, 0,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (DumpFile != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION DumpInfo = { 0 };
        DumpInfo.ClientPointers		= TRUE;
        DumpInfo.ExceptionPointers	= ExceptionData;
        DumpInfo.ThreadId			= GetCurrentThreadId();

        MINIDUMP_TYPE DumpType = (MINIDUMP_TYPE)(MiniDumpWithPrivateReadWriteMemory | 
                                                 MiniDumpWithThreadInfo |
                                                 MiniDumpWithUnloadedModules); 

        if (MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), DumpFile,
                               DumpType, &DumpInfo, 0, 0))
        {
            return DumpFile;
        }        
    }

    return INVALID_HANDLE_VALUE;
}

static LONG __stdcall ExceptionFilter(EXCEPTION_POINTERS* ExceptionData)
{
    HANDLE DumpFileHandle = CreateMiniDump(CrashContext.DumpFilePath, ExceptionData);

    if (CrashContext.ExceptionCallback)
    {
        exception_payload Payload;
        Payload.DumpFile = DumpFileHandle;
        Payload.ExceptionData = ExceptionData;
  
        CrashContext.ExceptionCallback(Payload);
    }

    if (DumpFileHandle && DumpFileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(DumpFileHandle);
    }
    
    return EXCEPTION_CONTINUE_SEARCH;
}


void InitializeCrashHandler(const char* DumpPath, exception_callback* ExceptionCallback)
{
    if (DumpPath)
    {
        strcpy_s(CrashContext.DumpFilePath, MAX_PATH, DumpPath);
    }
    else
    {
        strcpy_s(CrashContext.DumpFilePath, MAX_PATH, "crash.dmp");
    }

    CrashContext.ExceptionCallback = ExceptionCallback;
    
    SetUnhandledExceptionFilter(ExceptionFilter);
}

#endif
