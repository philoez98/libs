/*
    Small memory leak-checker, memory usage tracker, with optional stack trace collection.

    This a STB-style library, so do this:
    
        #define MEM_TRACK_IMPLEMENTATION
    in *one* C or C++ source file before including this header to create the implementation.
    Something like:

        #include ...
        #include ...
        #define MEM_TRACK_IMPLEMENTATION
        #include "mem_track.h"
     
    Additionally you can define, before including this header:
        #define MEM_TRACK_STATIC
    to have all functions declared as static.
        #define MEM_TRACK_ENABLE_STACKTRACE
    to capture the stack trace of each allocation (Windows only).
    
    NOTE: to be able to inspect the stack trace the code needs to be compiled
    with debug information enabled (-g for clang, /Zi for MSVC, ...).
    
    You can also provide alternate definitions of C library functions:
        #define MTALLOC(x)
        #define MTREALLOC(p, x)
        #define MTFREE(p) 
        #define MTPRINT(format, ...)
        
    
    Example usage:
    
    #define MEM_TRACK_IMPLEMENTATION
    #include "mem_track.h"

    int main() {

        void* p = MTAlloc(100);
        ...
        MTFree(p);

        unsigned long long int UsedMemory = MTGetUsedMemory();
        unsigned long long int LeakedMemory = MTGetLeakedMemory();

        return 0;
    }

    if MEM_TRACK_ENABLE_STACKTRACE is defined, you can inspect and print the allocations stack trace with
    MTPrintStackTrace(void* Ptr) or MTPrintFullStackTrace()
    
    If MTPRINT has not been defined the stack trace will be printed to stdout using printf, otherwise using the provided print function.
 */


#ifndef MEM_TRACK_H
#define MEM_TRACK_H

#ifndef MEM_TRACK_DEF
#ifdef MEM_TRACK_STATIC
#define MEM_TRACK_DEF static
#else

#ifdef __cplusplus
#define MEM_TRACK_DEF extern "C"
#else
#define MEM_TRACK_DEF extern
#endif

#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char uint8;
typedef unsigned int uint32;
typedef unsigned long long int uint64;

#ifdef MEM_TRACK_ENABLE_STACKTRACE

#if !defined(_WIN32) && !defined(_MSC_VER) && !defined(__WIN32__)
#error "Stack trace captures are available on Windows only."
#endif
    
#define MAX_STACKTRACE_SIZE 16
#endif

typedef struct mem_node {
    uint64 Size;

#ifdef MEM_TRACK_ENABLE_STACKTRACE
    void* StackTrace[MAX_STACKTRACE_SIZE];
    uint8 StackTraceCount;
#endif

   struct mem_node* Prev, *Next;
} mem_node;

typedef struct {
    uint32 AllocCount;
    uint32 ReallocCount;
    uint32 FreeCount;

    uint64 BytesUsed;
    uint64 BytesFreed;

    uint64 MaxAllocSize;
} mem_usage_info;

MEM_TRACK_DEF void* MTAlloc(uint64 Size);
MEM_TRACK_DEF void* MTRealloc(void* Ptr, uint64 Size);
MEM_TRACK_DEF void MTFree(void* Ptr);

MEM_TRACK_DEF uint64 MTGetUsedMemory();

// Prints the amount of memory allocated at the provided address
MEM_TRACK_DEF uint64 MTGetAddressSize(void* Ptr);
    
MEM_TRACK_DEF uint64 MTGetLeakedMemory();
MEM_TRACK_DEF mem_usage_info* MTGetMemoryUsage();
    
MEM_TRACK_DEF float MTGetAvgAllocationSize();

#ifdef MEM_TRACK_ENABLE_STACKTRACE

MEM_TRACK_DEF void MTPrintStackTrace(void* Ptr);
MEM_TRACK_DEF void MTPrintFullStackTrace();
    
#endif


#ifdef MEM_TRACK_IMPLEMENTATION

#if !defined(MTALLOC) || !defined(MTREALLOC) || !defined(MTFREE)
#include <stdlib.h>
#endif

#ifndef MTALLOC
#define MTALLOC(Size) malloc(Size)
#endif

#ifndef MTREALLOC
#define MTREALLOC(Ptr, Size) realloc(Ptr, Size)
#endif

#ifndef MTFREE
#define MTFREE(Ptr) free(Ptr)
#endif

#ifdef MEM_TRACK_ENABLE_STACKTRACE

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
    
#include <windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")

#ifndef MTPRINT
#include <stdio.h>
#define MTPRINT printf
#endif

#endif

#include <assert.h>

static mem_node* Head = 0;
static mem_usage_info UsageInfo = {0};

typedef long long int int64;

#define Max(a, b) (a) > (b) ? (a) : (b)

MEM_TRACK_DEF void* MTAlloc(uint64 Size) {
    mem_node* Node = (mem_node*)MTALLOC(Size + sizeof(mem_node));
    assert(Node != NULL);

    Node->Next = Node->Prev = 0;
    Node->Size = Size;

#ifdef MEM_TRACK_ENABLE_STACKTRACE
    Node->StackTraceCount = CaptureStackBackTrace(1, MAX_STACKTRACE_SIZE, Node->StackTrace, 0);
#endif

    if (Head) {
        Head->Prev = Node;
        Node->Next = Head;
    }

    Head = Node;

    UsageInfo.AllocCount += 1;
    UsageInfo.BytesUsed += Size;
    UsageInfo.MaxAllocSize = Max(Size, UsageInfo.MaxAllocSize);

    return (uint8*)Node + sizeof(mem_node);
}

MEM_TRACK_DEF void* MTRealloc(void* Ptr, uint64 Size) {

    if (!Ptr) {
        return MTAlloc(Size);
    }
    else if (Size == 0) {
        MTFree(Ptr);
        return NULL;
    }

    mem_node* OldPtr = (mem_node*)((uint8*)Ptr - sizeof(mem_node));
    assert(OldPtr && OldPtr->Size);

    uint32 OldSize = OldPtr->Size;

    mem_node* Node = (mem_node*)MTREALLOC(OldPtr, Size + sizeof(mem_node));
    assert(Node != NULL);

    Node->Size = Size;
    
    if (Head == OldPtr) Head = Node;
    if (Node->Prev) Node->Prev->Next = Node;
    if (Node->Next) Node->Next->Prev = Node;

    UsageInfo.ReallocCount += 1;

    int64 BytesAdded = (int64)Node->Size - OldSize;
    UsageInfo.BytesUsed += BytesAdded;

    return (uint8*)Node + sizeof(mem_node);
}

MEM_TRACK_DEF void MTFree(void* Ptr) {

    if (Ptr) {
        mem_node* Node = (mem_node*)((uint8*)Ptr - sizeof(mem_node));

        if (Node == Head) Head = Node->Next;
        if (Node->Prev) Node->Prev->Next = Node->Next;
        if (Node->Next) Node->Next->Prev = Node->Prev;

        UsageInfo.FreeCount += 1;
        UsageInfo.BytesFreed += Node->Size;

        MTFREE(Node);
    }
}

MEM_TRACK_DEF uint64 MTGetUsedMemory() {
    return UsageInfo.BytesUsed;
}

MEM_TRACK_DEF uint64 MTGetAddressSize(void* Ptr) {
    mem_node* Node = (mem_node*)((uint8*)Ptr - sizeof(mem_node));
    if (!Node) return 0;

    return Node->Size;
}

MEM_TRACK_DEF uint64 MTGetLeakedMemory() {
    return UsageInfo.BytesUsed - UsageInfo.BytesFreed;
}

MEM_TRACK_DEF mem_usage_info* MTGetMemoryUsage() {
    return &UsageInfo;
}

MEM_TRACK_DEF float MTGetAvgAllocationSize() {
    float AvgAlloc = 0;
    uint64 AllocCount = UsageInfo.AllocCount + UsageInfo.ReallocCount;
    
    if (AllocCount > 0) {
        AvgAlloc = UsageInfo.BytesUsed / (float)AllocCount;
    }

    return AvgAlloc;
}


#ifdef MEM_TRACK_ENABLE_STACKTRACE

MEM_TRACK_DEF void MTPrintStackTrace(void* Ptr) {
    mem_node* Node = (mem_node*)((uint8*)Ptr - sizeof(mem_node));
    if (!Node || !Node->StackTraceCount) return;

    HANDLE Process = GetCurrentProcess();

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    if (!SymInitialize(Process, 0, TRUE)) return;

    MTPRINT("Allocated %lluB (%.2fKB) at %p:\n", Node->Size, Node->Size / 1024.f, (uint8*)Node + sizeof(mem_node));

    IMAGEHLP_LINE64 Info;
    Info.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    for (int i = 0; i < Node->StackTraceCount; ++i) {
        void* Trace = Node->StackTrace[i];

        DWORD Disp;
        if (SymGetLineFromAddr64(Process, (DWORD64)Trace, &Disp, &Info)) {
            MTPRINT("\tfrom '%s' at line %lu:%lu\n", Info.FileName, Info.LineNumber, Disp);
        }
    }

    SymCleanup(Process);
}

MEM_TRACK_DEF void MTPrintFullStackTrace() {
    mem_node* Node = Head;
    HANDLE Process = GetCurrentProcess();

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    if (!SymInitialize(Process, 0, TRUE)) return;

    IMAGEHLP_LINE64 Info;
    Info.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    MTPRINT("Full stack trace:\n");

    while (Node) {

        MTPRINT("  - Allocated %lluB (%.2fKB) at %p\n", Node->Size, Node->Size / 1024.f, (uint8*)Node + sizeof(mem_node));

        for (int i = 0; i < Node->StackTraceCount; ++i) {
            void* Trace = Node->StackTrace[i];

            DWORD Disp;
            if (SymGetLineFromAddr64(Process, (DWORD64)Trace, &Disp, &Info)) {
                MTPRINT("    \tfrom '%s' at line %lu:%lu\n", Info.FileName, Info.LineNumber, Disp);
            }
        }

        Node = Node->Next;
    }

    SymCleanup(Process);
}
    
#endif


#endif // MEM_TRACK_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
    
#endif
