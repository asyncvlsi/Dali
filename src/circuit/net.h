//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_NET_HPP
#define HPCC_NET_HPP

#include <string>
#include <vector>
#include "block.h"
#include "blockpinpair.h"

class Net {
protected:
  std::string name_;
  int num_;
  double weight_;
public:
  explicit Net(std::string &name_arg, int num, double weight_arg = 1);
  std::vector<BlockPinPair> pin_list;

  friend std::ostream& operator<<(std::ostream& os, const Net &net) {
    os << net.name_ << "\n";
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
  bool add_pin(Pin &pin);
  double inv_p();
  int p();

  bool AddBlockPinPair(int block_index, int pin_index);

  int hpwl();
};


#endif //HPCC_NET_HPP
