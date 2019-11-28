//
// Created by Yihang Yang on 11/28/19.
//

#ifndef HPCC_SRC_CIRCUIT_IOPIN_H_
#define HPCC_SRC_CIRCUIT_IOPIN_H_

#include <string>
#include "net.h"

class iopin {
 private:
  std::string name_;
  std::string *net_name_;
  Net *net_;
  std::string *layer_;

 public:
};

#endif //HPCC_SRC_CIRCUIT_IOPIN_H_
