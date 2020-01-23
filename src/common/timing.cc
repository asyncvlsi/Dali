//
// Created by Yihang Yang on 1/22/20.
//

#include "timing.h"

//  Windows
#if defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__)
#include <Windows.h>
double get_wall_time(){
    LARGE_INTEGER time,freq;
    if (!QueryPerformanceFrequency(&freq)){
        //  Handle error
        return 0;
    }
    if (!QueryPerformanceCounter(&time)){
        //  Handle error
        return 0;
    }
    return (double)time.QuadPart / freq.QuadPart;
}

double get_cpu_time(){
    FILETIME a,b,c,d;
    if (GetProcessTimes(GetCurrentProcess(),&a,&b,&c,&d) != 0){
        //  Returns total user time.
        //  Can be tweaked to include kernel times as well.
        return
            (double)(d.dwLowDateTime |
            ((unsigned long long)d.dwHighDateTime << 32)) * 0.0000001;
    }else{
        //  Handle error
        return 0;
    }
}

//  Posix/Linux
#else

#include <sys/time.h>
double get_wall_time() {
  timeval time;
  if (gettimeofday(&time, nullptr)) {
    //  Handle error
    return 0;
  }
  return (double) time.tv_sec + (double) time.tv_usec * 0.000001;
}

#endif
