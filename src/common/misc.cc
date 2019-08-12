//
// Created by Yihang Yang on 2019-07-25.
//

#include "misc.h"

void Assert(bool e, const std::string &error_message) {
  if (!e) {
    std::cout << "FATAL ERROR:" << std::endl;
    std::cout << "\t" << error_message << std::endl;
    assert(e);
  }
}

void Warning(bool e, const std::string &warning_message) {
  if (e) {
    std::cout << "WARNING:" << std::endl;
    std::cout << "\t" << warning_message << std::endl;
  }
}