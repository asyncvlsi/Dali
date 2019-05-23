//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_CIRCUITNET_HPP
#define HPCC_CIRCUITNET_HPP

#include <string>
#include <vector>
#include "circuitpin.hpp"

class net_t {
private:
  /* essential data entries */
  std::string _name; // name

  /* the following entries are derived data */
  size_t _num;
  /* net_num is the index of this block in the vector net_list, this data must be updated after push a new block into net_list */
public:
  net_t();

  explicit net_t(std::string &name);

  /* essential data entries */
  std::vector<pin_t> pin_list;
  // the list of pins in the net. It is public because of easy accessibility.
  bool add_pin(pin_t &pin);
};


#endif //HPCC_CIRCUITNET_HPP
