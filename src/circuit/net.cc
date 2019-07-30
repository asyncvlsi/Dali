//
// Created by Yihang Yang on 2019-05-23.
//

#include "net.h"

Net::Net(std::pair<std::string,int> *name_num_pair_ptr, double weight): name_num_pair_ptr_(name_num_pair_ptr), weight_(weight) {}

void Net::AddBlockPinPair(Block *block_ptr, int pin_index) {
  blk_pin_pair_list.emplace_back(block_ptr, pin_index);
}

const std::string *Net::Name() const {
  return &(name_num_pair_ptr_->first);
}

size_t Net::Num() {
 return name_num_pair_ptr_->second;
}

void Net::SetWeight(double weight) {
  weight_ = weight;
}

double Net::Weight() {
  return weight_;
}

double Net::InvP() {
  Assert(blk_pin_pair_list.size() > 1, "Invalid net to calculate 1/(P-1)");
  return 1.0/(double)(blk_pin_pair_list.size() - 1);
}

int Net::P() {
  return (int)blk_pin_pair_list.size();
}

double Net::HPWL() {
  return 0;
}

