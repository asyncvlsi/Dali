//
// Created by Yihang Yang on 2019-08-05.
//

#include "netaux.h"

NetAux::NetAux(Net *net): net_(net) {
  net_->SetAux(this);
}

Net *NetAux::GetNet() {
  return net_;
}
