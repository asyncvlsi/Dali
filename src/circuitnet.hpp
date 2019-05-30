//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_CIRCUITNET_HPP
#define HPCC_CIRCUITNET_HPP

#include <string>
#include <vector>
#include "circuitpin.hpp"

class net_t {
protected:
  /* essential data entries */
  std::string _name; // name
  double _weight; // weight of this net

  /* the following entries are derived data */
  size_t _num;
  /* net_num is the index of this block in the vector net_list, this data must be updated after push a new block into net_list */
public:
  net_t();
  net_t(std::string &name, double weight=1);

  /* essential data entries */
  std::vector<pin_t> pin_list;
  // the list of pins in the net. It is public because of easy accessibility.

  friend std::ostream& operator<<(std::ostream& os, const net_t &net) {
    os << net._name <<"\n";
    for (auto &&pin: net.pin_list) {
      os << "\t" << pin << "\n";
    }
    return os;
  }

  void set_name(std::string &name);
  std::string name();
  void set_num(size_t num);
  size_t num();
  void set_weight(double weight);
  double weight();
  bool add_pin(pin_t &pin);

  int hpwl();
};


#endif //HPCC_CIRCUITNET_HPP
