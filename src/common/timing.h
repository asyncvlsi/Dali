//
// Created by Yihang Yang on 1/22/20.
//

#ifndef DALI_SRC_COMMON_TIMING_H_
#define DALI_SRC_COMMON_TIMING_H_

double get_wall_time();
#if defined(_WIN32) || defined(__MINGW32__) || defined(__CYGWIN__)
double get_cpu_time();
#else
#include <time.h>
inline double get_cpu_time() {
  return (double) clock() / CLOCKS_PER_SEC;
}
#endif

#endif //DALI_SRC_COMMON_TIMING_H_
