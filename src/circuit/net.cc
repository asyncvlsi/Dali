//
// Created by Yihang Yang on 2019-05-23.
//

#include <cfloat>

#include <algorithm>

#include "common/misc.h"
#include "net.h"

Net::Net(std::pair<const std::string, int> *name_num_pair_ptr, int capacity, double weight)
    : name_num_pair_ptr_(name_num_pair_ptr), weight_(weight) {
  cnt_fixed_ = 0;
  max_pin_x_ = -1;
  min_pin_x_ = -1;
  max_pin_y_ = -1;
  min_pin_y_ = -1;
  inv_p_ = 0;
  aux_ptr_ = nullptr;
  blk_pin_list.reserve(capacity);
}

void Net::AddBlockPinPair(Block *block_ptr, Pin *pin_ptr) {
  if (blk_pin_list.size() < blk_pin_list.capacity()) {
    blk_pin_list.emplace_back(block_ptr, pin_ptr);
    if (!(pin_ptr->IsInput())) driver_pin_index = int(blk_pin_list.size())-1;
    if (!block_ptr->IsMovable()) {
      ++cnt_fixed_;
    }
    // because net list is stored as a vector, so the location of a net will change, thus here, we have to use Num() to
    // find a net, although a pointer to this net is more convenient.
    block_ptr->net_list_.push_back(Num());
    int p_minus_one = int(blk_pin_list.size()) - 1;
    inv_p_ = p_minus_one > 0 ? 1.0 * weight_ / p_minus_one : 0;
  } else {
    std::cout << "Pre-assigned net capacity is full: " << blk_pin_list.capacity() << ", cannot add more pin to this net:\n";
    std::cout << "net name: " << *Name() << ", net weight: " << Weight() << "\n";
    for (auto &block_pin_pair: blk_pin_list) {
      std::cout << "\t" << " (" << *(block_pin_pair.BlockNamePtr()) << " " << *(block_pin_pair.PinNamePtr()) << ") " << "\n";
    }
    exit(1);
  }
}

void Net::XBoundExclude(Block *blk_ptr, double &lo, double &hi) {
  /****
   *
   * ****/
  lo = DBL_MIN;
  hi = DBL_MAX;

  int sz = blk_pin_list.size();
  if (sz == 1) {
    return;
  }

  double max_x = DBL_MIN;
  double min_x = DBL_MAX;
  double tmp_pin_loc;

  for (auto &pair: blk_pin_list) {
    if (pair.BlkPtr() == blk_ptr) continue;
    tmp_pin_loc = pair.AbsX();
    if (max_x < tmp_pin_loc) {
      max_x = tmp_pin_loc;
    }
    if (min_x > tmp_pin_loc) {
      min_x = tmp_pin_loc;
    }
  }

  if (min_x <= max_x) {
    lo = min_x;
    hi = max_x;
  }
}

void Net::YBoundExclude(Block *blk_ptr, double &lo, double &hi) {
  lo = DBL_MIN;
  hi = DBL_MAX;

  int sz = blk_pin_list.size();
  if (sz == 1) {
    return;
  }

  double max_y = DBL_MIN;
  double min_y = DBL_MAX;
  double tmp_pin_loc;

  for (auto &pair: blk_pin_list) {
    if (pair.BlkPtr() == blk_ptr) continue;
    tmp_pin_loc = pair.AbsY();
    if (max_y < tmp_pin_loc) {
      max_y = tmp_pin_loc;
    }
    if (min_y > tmp_pin_loc) {
      min_y = tmp_pin_loc;
    }
  }
  if (min_y <= max_y) {
    lo = min_y;
    hi = max_y;
  }
}

void Net::SortBlkPinList() {
  std::sort(blk_pin_list.begin(), blk_pin_list.end());
}

void Net::UpdateMaxMinIndexX() {
  if (blk_pin_list.empty()) return;
  max_pin_x_ = 0;
  min_pin_x_ = 0;
  double max_x = blk_pin_list[0].AbsX();
  double min_x = max_x;
  double tmp_pin_loc = 0;
  for (size_t i = 1; i < blk_pin_list.size(); ++i) {
    tmp_pin_loc = blk_pin_list[i].AbsX();
    if (max_x < tmp_pin_loc) {
      max_x = tmp_pin_loc;
      max_pin_x_ = i;
    }
    if (min_x > tmp_pin_loc) {
      min_x = tmp_pin_loc;
      min_pin_x_ = i;
    }
  }
}

void Net::UpdateMaxMinIndexY() {
  if (blk_pin_list.empty()) return;
  max_pin_y_ = 0;
  min_pin_y_ = 0;
  double max_y = blk_pin_list[0].AbsY();
  double min_y = max_y;
  double tmp_pin_loc = 0;
  for (size_t i = 1; i < blk_pin_list.size(); ++i) {
    tmp_pin_loc = blk_pin_list[i].AbsY();
    if (max_y < tmp_pin_loc) {
      max_y = tmp_pin_loc;
      max_pin_y_ = i;
    }
    if (min_y > tmp_pin_loc) {
      min_y = tmp_pin_loc;
      min_pin_y_ = i;
    }
  }
}

double Net::WeightedHPWLX() {
  if (blk_pin_list.size() <= 1) return 0;
  UpdateMaxMinIndexX();
  double max_x = blk_pin_list[max_pin_x_].AbsX();
  double min_x = blk_pin_list[min_pin_x_].AbsX();
  return (max_x - min_x) * weight_;
}

double Net::WeightedHPWLY() {
  if (blk_pin_list.size() <= 1) return 0;
  UpdateMaxMinIndexY();
  double max_y = blk_pin_list[max_pin_y_].AbsY();
  double min_y = blk_pin_list[min_pin_y_].AbsY();
  return (max_y - min_y) * weight_;
}

void Net::UpdateMaxMinCtoCX() {
  if (blk_pin_list.empty()) return;
  max_pin_x_ = 0;
  min_pin_x_ = 0;
  double max_x = blk_pin_list[0].BlkPtr()->X();
  double min_x = max_x;
  double tmp_pin_loc = 0;
  for (size_t i = 1; i < blk_pin_list.size(); ++i) {
    tmp_pin_loc = blk_pin_list[i].BlkPtr()->X();
    if (max_x < tmp_pin_loc) {
      max_x = tmp_pin_loc;
      max_pin_x_ = i;
    }
    if (min_x > tmp_pin_loc) {
      min_x = tmp_pin_loc;
      min_pin_x_ = i;
    }
  }
}

void Net::UpdateMaxMinCtoCY() {
  if (blk_pin_list.empty()) return;
  max_pin_y_ = 0;
  min_pin_y_ = 0;
  double max_y = blk_pin_list[0].BlkPtr()->Y();
  double min_y = max_y;
  double tmp_pin_loc = 0;
  for (size_t i = 1; i < blk_pin_list.size(); ++i) {
    tmp_pin_loc = blk_pin_list[i].BlkPtr()->Y();
    if (max_y < tmp_pin_loc) {
      max_y = tmp_pin_loc;
      max_pin_y_ = i;
    }
    if (min_y > tmp_pin_loc) {
      min_y = tmp_pin_loc;
      min_pin_y_ = i;
    }
  }
}

void Net::UpdateMaxMinCtoC() {
  UpdateMaxMinIndexX();
  UpdateMaxMinIndexY();
}

int Net::MaxPinCtoCX() {
  size_t max_pin_index = 0;
  auto *block = blk_pin_list[0].BlkPtr();
  double max_x = block->X();
  for (size_t i = 0; i < blk_pin_list.size(); i++) {
    block = blk_pin_list[i].BlkPtr();
    if (max_x < block->X()) {
      max_x = block->X();
      max_pin_index = i;
    }
  }
  return max_pin_index;
}

int Net::MinPinCtoCX() {
  size_t min_pin_index = 0;
  auto *block = blk_pin_list[0].BlkPtr();
  double min_x = block->X();
  for (size_t i = 0; i < blk_pin_list.size(); i++) {
    block = blk_pin_list[i].BlkPtr();
    if (min_x > block->X()) {
      min_x = block->X();
      min_pin_index = i;
    }
  }
  return min_pin_index;
}

int Net::MaxPinCtoCY() {
  size_t max_pin_index = 0;
  auto *block = blk_pin_list[0].BlkPtr();
  double max_y = block->Y();
  for (size_t i = 0; i < blk_pin_list.size(); i++) {
    block = blk_pin_list[i].BlkPtr();
    if (max_y < block->Y()) {
      max_y = block->Y();
      max_pin_index = i;
    }
  }
  return max_pin_index;
}

int Net::MinPinCtoCY() {
  size_t min_pin_index = 0;
  auto *block = blk_pin_list[0].BlkPtr();
  double min_y = block->Y();
  for (size_t i = 0; i < blk_pin_list.size(); i++) {
    block = blk_pin_list[i].BlkPtr();
    if (min_y > block->Y()) {
      min_y = block->Y();
      min_pin_index = i;
    }
  }
  return min_pin_index;
}

double Net::HPWLCtoCX() {
  if (blk_pin_list.empty()) return 0;
  auto *block = blk_pin_list[0].BlkPtr();
  double max_x = block->X();
  double min_x = block->X();

  for (auto &pin: blk_pin_list) {
    if (pin.BlkPtr() == nullptr) {
      std::cout << "Error!\n";
      std::cout << "attribute block_t* _block is nullptr, it should points to the block containing this pin\n";
      assert(pin.BlkPtr() != nullptr);
    }
    block = pin.BlkPtr();
    if (max_x < block->X()) {
      max_x = block->X();
    }
    if (min_x > block->X()) {
      min_x = block->X();
    }
  }

  return (max_x - min_x) * weight_;
}

double Net::HPWLCtoCY() {
  if (blk_pin_list.empty()) return 0;
  auto *block = blk_pin_list[0].BlkPtr();
  double max_y = block->Y();
  double min_y = block->Y();

  for (auto &pin: blk_pin_list) {
    if (pin.BlkPtr() == nullptr) {
      std::cout << "Error!\n";
      std::cout << "attribute block_t* _block is nullptr, it should points to the block containing this pin\n";
      assert(pin.BlkPtr() != nullptr);
    }
    block = pin.BlkPtr();
    if (max_y < block->Y()) {
      max_y = block->Y();
    }
    if (min_y > block->Y()) {
      min_y = block->Y();
    }
  }

  return (max_y - min_y) * weight_;
}
