//
// Created by Yihang Yang on 2019-07-25.
//

#include "misc.h"

void Assert(bool e, const std::string &error_message) {
  if (!e) {
    std::cout << "\033[0;31m" << "FATAL ERROR:" << "\n"
              << "    " << error_message << "\033[0m" << std::endl;
    exit(1);
  }
}

void Warning(bool e, const std::string &warning_message) {
  if (globalVerboseLevel >= LOG_WARNING) {
    if (e) {
      std::cout << "WARNING:" << "\n"
                << "    " << warning_message << std::endl;
    }
  }
}
