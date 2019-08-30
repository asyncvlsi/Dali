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
  if (globalVerboseLevel >= LOG_WARNING) {
    if (e) {
      std::cout << "WARNING:" << std::endl;
      std::cout << "\t" << warning_message << std::endl;
    }
  }
}

void VerbosePrint(VerboseLevel verbose_level, std::stringstream &buf) {
  /**** this function is easy to use, but might not be efficient, because whether or not a log is printed out,
   * the stringstream buf should always be generated, if not used, the computation for this buffer is wasted ****/
  if (globalVerboseLevel >= verbose_level) {
    std::cout << buf.str();
  }
  buf.str(std::string()); // clear the buf
}
