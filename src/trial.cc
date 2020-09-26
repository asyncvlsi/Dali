//
// Created by yihang on 9/22/20.
//

#include <vector>

#include <galois/Galois.h>
#include <galois/LargeArray.h>

#include "common/timing.h"

int main() {
  int num_of_thread = 4;
  galois::SharedMemSys G;
  galois::setActiveThreads(num_of_thread);
  //initialize(g);
  size_t N = 2 << 22;
  size_t step = N/num_of_thread;
  printf("%d %d\n", N, step);
  galois::LargeArray<int> g;
  g.allocateBlocked(N);
  for (size_t i=0; i<step; ++i) {
    g[i] = i;
  }

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();
  galois::do_all(
      galois::iterate(size_t{0}, size_t{size_t(num_of_thread)}), // range
      [&](size_t n) {
        for (size_t i=n*step; i<(n+1)*step; ++i) {
          ++g[i];
        }
      } // operator
      ,
      galois::loopname("sum_in_for_each_with_push_atomic") // options
      ,
      galois::no_pushes(), galois::no_conflicts());
//  for (auto &num: g) {
//    ++num;
//  }

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  printf("%d\n", g[0]);
  printf("****End of execution (wall time: %.4fs, cpu time: %.4fs)****\n", wall_time, cpu_time);

  return 0;
}