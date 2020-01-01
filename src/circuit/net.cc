//
// Created by Yihang Yang on 2019-05-23.
//

#include <algorithm>

#include "common/misc.h"
#include "net.h"

Net::Net(std::pair<const std::string, int> *name_num_pair_ptr, double weight)
    : name_num_pair_ptr_(name_num_pair_ptr), weight_(weight) {
  cnt_fixed_ = 0;
  max_pin_x_ = -1;
  min_pin_x_ = -1;
  max_pin_y_ = -1;
  min_pin_y_ = -1;
  aux_ = nullptr;
}

const std::string *Net::Name() const {
  return &(name_num_pair_ptr_->first);
}

int Net::Num() {
  return name_num_pair_ptr_->second;
}

void Net::SetWeight(double weight) {
  weight_ = weight;
}

double Net::Weight() const {
  return weight_;
}

double Net::InvP() {
  Assert(blk_pin_list.size() > 1, "Invalid net to calculate 1/(P-1)");
  return 1.0 * weight_ / (double) (blk_pin_list.size() - 1);
}

int Net::P() {
  return (int) blk_pin_list.size();
}

int Net::FixedCnt() {
  return cnt_fixed_;
}

void Net::SetAux(NetAux *aux) {
  Assert(aux != nullptr, "When set auxiliary information, argument cannot be a nullptr");
  aux_ = aux;
}

NetAux *Net::Aux() {
  return aux_;
}

int Net::MaxBlkPinNumX() {
  return max_pin_x_;
}

int Net::MinBlkPinNumX() {
  return min_pin_x_;
}

int Net::MaxBlkPinNumY() {
  return max_pin_y_;
}

int Net::MinBlkPinNumY() {
  return min_pin_y_;
}

Block *Net::MaxBlockX() {
  return blk_pin_list[max_pin_x_].GetBlock();
}

Block *Net::MinBlockX() {
  return blk_pin_list[min_pin_x_].GetBlock();
}

Block *Net::MaxBlockY() {
  return blk_pin_list[max_pin_y_].GetBlock();
}

Block *Net::MinBlockY() {
  return blk_pin_list[min_pin_y_].GetBlock();
}

void Net::AddBlockPinPair(Block *block_ptr, int pin_index) {
  blk_pin_list.emplace_back(block_ptr, pin_index);
  if (!block_ptr->IsMovable()) {
    ++cnt_fixed_;
  }
  // because net list is stored as a vector, so the location of a net will change, thus here, we have to use Num() to
  // find a net, although a pointer to this net is more convenient.
  block_ptr->net_list.push_back(Num());
}

void Net::XBoundExclude(Block *blk_ptr, double &x1, double &x2) {
  double db_max = 1e30;
  x1 = -db_max;
  x2 = db_max;
  if (blk_pin_list.size() == 1) return;
  max_pin_x_ = -1;
  min_pin_x_ = -1;
  double max_x = -db_max;
  double min_x = db_max;
  double tmp_pin_loc;
  for (size_t i = 0; i < blk_pin_list.size(); ++i) {
    if (blk_pin_list[i].GetBlock() == blk_ptr) continue;
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
  x1 = blk_pin_list[min_pin_x_].AbsX();
  x2 = blk_pin_list[max_pin_x_].AbsX();
}

void Net::YBoundExclude(Block *blk_ptr, double &y1, double &y2) {
  double db_max = 1e30;
  y1 = -db_max;
  y2 = db_max;
  if (blk_pin_list.size() == 1) return;
  max_pin_y_ = -1;
  min_pin_y_ = -1;
  double max_y = -db_max;
  double min_y = db_max;
  double tmp_pin_loc;
  for (size_t i = 0; i < blk_pin_list.size(); i++) {
    if (blk_pin_list[i].GetBlock() == blk_ptr) continue;
    tmp_pin_loc = blk_pin_list[i].GetBlock()->LLY() + blk_pin_list[i].YOffset();
    if (max_y < tmp_pin_loc) {
      max_y = tmp_pin_loc;
      max_pin_y_ = i;
    }
    if (min_y > tmp_pin_loc) {
      min_y = tmp_pin_loc;
      min_pin_y_ = i;
    }
  }
  y1 = blk_pin_list[min_pin_y_].AbsY();
  y2 = blk_pin_list[max_pin_y_].AbsY();
}

void Net::SortBlkPinList() {
  std::sort(blk_pin_list.begin(), blk_pin_list.end());
}

void Net::UpdateMaxMinX() {
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

void Net::UpdateMaxMinY() {
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

void Net::UpdateMaxMin() {
  UpdateMaxMinX();
  UpdateMaxMinY();
}

double Net::HPWLX() {
  UpdateMaxMinX();
  double max_x = blk_pin_list[max_pin_x_].AbsX();
  double min_x = blk_pin_list[min_pin_x_].AbsX();
  return (max_x - min_x) * weight_;
}

double Net::HPWLY() {
  UpdateMaxMinY();
  double max_y = blk_pin_list[max_pin_y_].AbsY();
  double min_y = blk_pin_list[min_pin_y_].AbsY();
  return (max_y - min_y) * weight_;
}

double Net::HPWL() {
  return HPWLX() + HPWLY();
}

void Net::UpdateMaxMinCtoCX() {
  max_pin_x_ = 0;
  min_pin_x_ = 0;
  double max_x = blk_pin_list[0].GetBlock()->X();
  double min_x = max_x;
  double tmp_pin_loc = 0;
  for (size_t i = 1; i < blk_pin_list.size(); ++i) {
    tmp_pin_loc = blk_pin_list[i].GetBlock()->X();
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
  max_pin_y_ = 0;
  min_pin_y_ = 0;
  double max_y = blk_pin_list[0].GetBlock()->Y();
  double min_y = max_y;
  double tmp_pin_loc = 0;
  for (size_t i = 1; i < blk_pin_list.size(); ++i) {
    tmp_pin_loc = blk_pin_list[i].GetBlock()->Y();
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
  UpdateMaxMinX();
  UpdateMaxMinY();
}

int Net::MaxPinCtoCX() {
  size_t max_pin_index = 0;
  auto *block = blk_pin_list[0].GetBlock();
  double max_x = block->X();
  for (size_t i = 0; i < blk_pin_list.size(); i++) {
    block = blk_pin_list[i].GetBlock();
    if (max_x < block->X()) {
      max_x = block->X();
      max_pin_index = i;
    }
  }
  return max_pin_index;
}

int Net::MinPinCtoCX() {
  size_t min_pin_index = 0;
  auto *block = blk_pin_list[0].GetBlock();
  double min_x = block->X();
  for (size_t i = 0; i < blk_pin_list.size(); i++) {
    block = blk_pin_list[i].GetBlock();
    if (min_x > block->X()) {
      min_x = block->X();
      min_pin_index = i;
    }
  }
  return min_pin_index;
}

int Net::MaxPinCtoCY() {
  size_t max_pin_index = 0;
  auto *block = blk_pin_list[0].GetBlock();
  double max_y = block->Y();
  for (size_t i = 0; i < blk_pin_list.size(); i++) {
    block = blk_pin_list[i].GetBlock();
    if (max_y < block->Y()) {
      max_y = block->Y();
      max_pin_index = i;
    }
  }
  return max_pin_index;
}

int Net::MinPinCtoCY() {
  size_t min_pin_index = 0;
  auto *block = blk_pin_list[0].GetBlock();
  double min_y = block->Y();
  for (size_t i = 0; i < blk_pin_list.size(); i++) {
    block = blk_pin_list[i].GetBlock();
    if (min_y > block->Y()) {
      min_y = block->Y();
      min_pin_index = i;
    }
  }
  return min_pin_index;
}

double Net::HPWLCtoCX() {
  Assert(!blk_pin_list.empty(), "Net contains no pin: " + *Name());
  auto *block = blk_pin_list[0].GetBlock();
  double max_x = block->X();
  double min_x = block->X();

  for (auto &&pin: blk_pin_list) {
    if (pin.GetBlock() == nullptr) {
      std::cout << "Error!\n";
      std::cout << "attribute block_t* _block is nullptr, it should points to the block containing this pin\n";
      assert(pin.GetBlock() != nullptr);
    }
    block = pin.GetBlock();
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
  Assert(!blk_pin_list.empty(), "Net contains no pin: " + *Name());
  auto *block = blk_pin_list[0].GetBlock();
  double max_y = block->Y();
  double min_y = block->Y();

  for (auto &&pin: blk_pin_list) {
    if (pin.GetBlock() == nullptr) {
      std::cout << "Error!\n";
      std::cout << "attribute block_t* _block is nullptr, it should points to the block containing this pin\n";
      assert(pin.GetBlock() != nullptr);
    }
    block = pin.GetBlock();
    if (max_y < block->Y()) {
      max_y = block->Y();
    }
    if (min_y > block->Y()) {
      min_y = block->Y();
    }
  }

  return (max_y - min_y) * weight_;
}

double Net::HPWLCtoC() {
  return HPWLCtoCX() + HPWLCtoCY();
}
