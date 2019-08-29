//
// Created by Yihang Yang on 2019-07-25.
//

#include "misc.h"

//#define USEDEGBUG
#include <cmath>

void Assert(bool e, const std::string &error_message) {
  if (!e) {
    std::cout << "FATAL ERROR:" << std::endl;
    std::cout << "\t" << error_message << std::endl;
    assert(e);
  }
}

void Warning(bool e, const std::string &warning_message) {
  #ifndef USEDEGBUG
  if (e) {
    std::cout << "WARNING:" << std::endl;
    std::cout << "\t" << warning_message << std::endl;
  }
  #endif
}

double Random() {
  // Schrage Method random number generator, a = 16807, q = 127773, r = 2836, m = 2^31 - 1 = 2147483647;
  static long seed = 0;
  double f = (16807*fmod(seed, 127773) - 2836*(seed/127773));
  if (f > 0) {
    seed = (long)(16807*fmod(seed, 127773)-2836*(seed/127773));
  } else {
    f += 1.0;
    seed = (long)(16807*fmod(seed, 127773)-2836*(seed/127773)) + 2147483647;
  }
  return f/2147483647.0;
}
