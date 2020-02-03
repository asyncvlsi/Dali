//
// Created by Yihang Yang on 1/22/20.
//

#ifndef DALI_SRC_COMMON_TIMING_H_
#define DALI_SRC_COMMON_TIMING_H_

double get_wall_time();

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

#endif //DALI_SRC_COMMON_TIMING_H_
