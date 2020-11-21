//
// Created by Yihang Yang on 1/22/20.
//

#ifndef DALI_SRC_COMMON_TIMING_H_
#define DALI_SRC_COMMON_TIMING_H_

namespace dali {

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
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__) || defined(__APPLE__)

#include <sys/time.h>
inline double get_wall_time() {
  timeval time;
  if (gettimeofday(&time, nullptr)) {
    //  Handle error
    return 0;
  }
  return (double) time.tv_sec + (double) time.tv_usec * 0.000001;
}

#else
double get_wall_time() {
  return 0;
}

#endif

#if defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__)
double get_cpu_time();
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__) || defined(__APPLE__)
#include <time.h>
inline double get_cpu_time() {
  return (double) clock() / CLOCKS_PER_SEC;
}
#else
inline double get_cpu_time() {
  return 0;
}
#endif

}

#endif //DALI_SRC_COMMON_TIMING_H_
