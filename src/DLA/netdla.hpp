//
// Created by Yihang Yang on 2019-06-01.
//

#ifndef HPCC_NETDLA_HPP
#define HPCC_NETDLA_HPP

#include <string>
#include "circuitnet.hpp"
#include "blockdla.hpp"

class net_dla_t: public net_t {
public:
  net_dla_t();
  explicit net_dla_t(std::string &name, double weight = 1);
  void retrieve_info_from_database(net_t &net);

  int hpwl_during_dla();
};


#endif //HPCC_NETDLA_HPP
