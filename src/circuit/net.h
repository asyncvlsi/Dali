//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_NET_HPP
#define HPCC_NET_HPP

#include <string>
#include <vector>
#include "block.h"
#include "blockpinpair.h"
#include "common/misc.h"

class Net {
 protected:
  std::pair<const std::string, int> *name_num_pair_ptr_;
  double weight_;
 public:
  explicit Net(std::pair<const std::string, int> *name_num_pair_ptr, double weight = 1);

  std::vector<BlockPinPair> blk_pin_pair_list;
  void AddBlockPinPair(Block *block_ptr, int pin_index);

  const std::string *Name() const;
  size_t Num();
  void SetWeight(double weight);
  double Weight();
  double InvP();
  int P();
  double HPWL();

  friend std::ostream& operator<<(std::ostream& os, const Net &net) {
    os << *net.Name() << "\n";
    for (auto &&block_pin_pair: net.blk_pin_pair_list) {
      os << "\t" << block_pin_pair << "\n";
    }
    return os;
  }
};


#endif //HPCC_NET_HPP