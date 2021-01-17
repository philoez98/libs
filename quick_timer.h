/*  
    A simple timer. Works only on Windows.
    
   
    This a STB-style library, so do this:
    
        #define QUICK_TIMER_IMPLEMENTATION
    in *one* C or C++ source file before including this header to create the implementation.
    Something like:

        #include ...
        #include ...
        #define QUICK_TIMER_IMPLEMENTATION
        #include "quick_timer.h"
     
    Additionally you can define, before including this header:
        #define QUICK_TIMER_STATIC
    to have all functions declared as static.
        
    Example usage:
    
    #define QUICK_TIMER_IMPLEMENTATION
    #include "quick_timer.h"

    int main() {

        quick_timer Timer;
        BeginTimer(&Timer);

        function_to_profile(); // Anything here, really.
        
        EndTimer(&Timer);

        double ElapsedTime = GetTimeSec(&Timer); // Get the result in seconds.

        return 0;
    }

    Additionally you can assign a name and/or a number (an identifier, the source code location, ...) to the timer, and
    access them directly within the struct. It is mostly useful when printing/debugging.
 */



#ifndef QUICK_TIMER_H
#define QUICK_TIMER_H

#ifndef QUICK_TIMER_DEF
#ifdef QUICK_TIMER_STATIC
#define QUICK_TIMER_DEF static
#else

#ifdef __cplusplus
#define QUICK_TIMER_DEF extern "C"
#else
#define QUICK_TIMER_DEF extern
#endif

#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long long int uint64;

typedef struct {
    const char* Name;
    int Line;

    uint64 CurrentFreq;
    uint64 CurrentTick;
} quick_timer;


#ifdef __cplusplus
QUICK_TIMER_DEF void BeginTimer(quick_timer* Timer, const char* Name = 0, int Line = 0);
#else
QUICK_TIMER_DEF void BeginTimer(quick_timer* Timer, const char* Name, int Line);
#endif

QUICK_TIMER_DEF void EndTimer(quick_timer* Timer);

QUICK_TIMER_DEF double GetTimeSec(quick_timer* Timer);
QUICK_TIMER_DEF double GetTimeMs(quick_timer* Timer);
QUICK_TIMER_DEF double GetTimeUs(quick_timer* Timer);
QUICK_TIMER_DEF double GetTimeNs(quick_timer* Timer);

// Macro for a single timer. If you need multiple timers inside a function, use BeginTimer/EndTimer directly.
// In END_TIMER() you need to pass the variable that will store the result, along with the wanted time units.
// The available time units are:
// - Sec  -> seconds
// - Ms   -> milliseconds
// - Us   -> microseconds
// - Ns   -> nanoseconds
// So, for example, you can do:
//
//  BEGIN_TIMER()
//  ...
//  END_TIMER(Time, Ms) // 'Time' will contain the result in milliseconds
//
// NOTE: the variable 'Time' doesn't have to be declared before calling the macro.
//
#define BEGIN_TIMER() quick_timer __Timer; BeginTimer(&__Timer, __FILE__, __LINE__);
#define END_TIMER(Result, units) EndTimer(&__Timer); double Result = GetTime##units(&__Timer);


#ifdef QUICK_TIMER_IMPLEMENTATION

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#define TICKS_PER_SEC 1000000000
#define TICKS_PER_MS  1000000
#define TICKS_PER_US  1000


// This is to avoid 64-bit integer overflow when computing the remaining time.
static inline uint64 ComputeRemainingTicks(uint64 Value, uint64 Num, uint64 Denom) {
    uint64 q = Value / Denom;
    uint64 r = Value % Denom;
    return q * Num + r * Num / Denom;
}

QUICK_TIMER_DEF void BeginTimer(quick_timer* Timer, const char* Name, int Line) {
    Timer->Name = Name ? Name : __FILE__;
    Timer->Line = Line ? Line : __LINE__;

    LARGE_INTEGER Time, Freq;
    QueryPerformanceFrequency(&Freq);
    QueryPerformanceCounter(&Time);

    Timer->CurrentFreq = Freq.QuadPart;
    Timer->CurrentTick = Time.QuadPart;
}

QUICK_TIMER_DEF void EndTimer(quick_timer* Timer) {
    LARGE_INTEGER Time;
    QueryPerformanceCounter(&Time);
    Timer->CurrentTick = ComputeRemainingTicks(Time.QuadPart - Timer->CurrentTick, TICKS_PER_SEC, Timer->CurrentFreq);
}

QUICK_TIMER_DEF double GetTimeSec(quick_timer* Timer) {
    return Timer->CurrentTick / (double)TICKS_PER_SEC;
}

QUICK_TIMER_DEF double GetTimeMs(quick_timer* Timer) {
    return Timer->CurrentTick / (double)TICKS_PER_MS;
}

QUICK_TIMER_DEF double GetTimeUs(quick_timer* Timer) {
    return Timer->CurrentTick / (double)TICKS_PER_US;
}

QUICK_TIMER_DEF double GetTimeNs(quick_timer* Timer) {
     return Timer->CurrentTick;
}


#endif // QUICK_TIMER_IMPLEMENTATION

    
#ifdef __cplusplus
}
#endif
    
#endif
