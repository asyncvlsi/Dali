//
// Created by Yihang Yang on 2019-08-05.
//

#ifndef HPCC_SRC_NETAUX_H_
#define HPCC_SRC_NETAUX_H_

#include "net.h"

class Net;

class NetAux {
 private:
  Net *net_;
 public:
  explicit NetAux(Net *net);
  Net *GetNet();
};

#endif //HPCC_SRC_PLACER_NETAUX_H_
