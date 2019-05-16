//
// Created by Yihang Yang on 2019-05-14.
//

#include <iostream>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <vector>

int main() {
  clock_t T=clock();




  T = clock() - T;
  std::cout << "Process complete. Execution time : " << (float)T/CLOCKS_PER_SEC << " s\n";
  return 0;
}