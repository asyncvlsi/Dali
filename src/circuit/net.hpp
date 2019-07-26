//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_NET_HPP
#define HPCC_NET_HPP

#include <string>
#include <vector>
#include "pin.h"

class net_t {
protected:
  /* essential data entries */
  std::string _name; // Name
  double _weight; // weight of this net

  /* the following entries are derived data */
  size_t _num;
  /* net_num is the index of this block in the vector net_list, this data must be updated after push a new block into net_list */
public:
  net_t();
  explicit net_t(std::string &name_arg, double weight_arg = 1);

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

  void set_name(const std::string &name_arg);
  std::string name();
  void set_num(size_t number);
  size_t num();
  void set_weight(double weight_arg);
  double weight();
  bool add_pin(pin_t &pin);
  double inv_p();
  int p();

  int hpwl();
};


#endif //HPCC_NET_HPP
