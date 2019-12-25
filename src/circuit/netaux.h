//
// Created by Yihang Yang on 2019-08-05.
//

#ifndef DALI_SRC_NETAUX_H_
#define DALI_SRC_NETAUX_H_

#include "net.h"

class Net;

class NetAux {
 protected:
  Net *net_;
 public:
  explicit NetAux(Net *net);
  Net *GetNet();
};

#endif //DALI_SRC_PLACER_NETAUX_H_
