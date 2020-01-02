//
// Created by Yihang Yang on 2019-06-01.
//

#ifndef DALI_NETDLA_HPP
#define DALI_NETDLA_HPP

#include <string>
#include "circuit/net.h"
#include "blockdla.hpp"

class net_dla_t: public net_t {
public:
  net_dla_t();
  explicit net_dla_t(std::string &name_arg, double weight_arg = 1);
  void retrieve_info_from_database(net_t &net);

  int hpwl_during_dla();
};


#endif //DALI_NETDLA_HPP
