//
// Created by Yihang Yang on 2019-05-23.
//

#include "net.h"

Net::Net(std::pair<const std::string, int> *name_num_pair_ptr, double weight): name_num_pair_ptr_(name_num_pair_ptr), weight_(weight) {
  max_pin_x_ = -1;
  min_pin_x_ = -1;
  max_pin_y_ = -1;
  min_pin_y_ = -1;
}

void Net::AddBlockPinPair(Block *block_ptr, int pin_index) {
  blk_pin_list.emplace_back(block_ptr, pin_index);
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

double Net::Weight() {
  return weight_;
}

double Net::InvP() {
  Assert(blk_pin_list.size() > 1, "Invalid net to calculate 1/(P-1)");
  return 1.0/(double)(blk_pin_list.size() - 1);
}

int Net::P() {
  return (int)blk_pin_list.size();
}


void Net::UpdateMaxMinX() {
  size_t max_pin_index = 0;
  auto *block = blk_pin_list[0].GetBlock();
  double max_x = block->LLX() + blk_pin_list[0].XOffset();
  for (size_t i=0; i<blk_pin_list.size(); i++) {
    block = blk_pin_list[i].GetBlock();
    if (max_x < block->LLX() + blk_pin_list[i].XOffset()) {
      max_x = block->LLX() + blk_pin_list[i].XOffset();
      max_pin_index = i;
    }
  }
  max_pin_x_ = max_pin_index;


}

void Net::UpdateMaxMinY() {
  size_t max_pin_index = 0;
  auto *block = blk_pin_list[0].GetBlock();
  double max_y = block->LLY() + blk_pin_list[0].YOffset();
  for (size_t i=0; i<blk_pin_list.size(); i++) {
    block = blk_pin_list[i].GetBlock();
    if (max_y < block->LLY() + blk_pin_list[i].YOffset()) {
      max_y = block->LLY() + blk_pin_list[i].YOffset();
      max_pin_index = i;
    }
  }
  max_pin_y_ = max_pin_index;
}

void Net::UpdateMaxMin() {
  UpdateMaxMinX();
  UpdateMaxMinY();
}

int Net::MaxPinX() {
  size_t max_pin_index = 0;
  auto *block = blk_pin_list[0].GetBlock();
  double max_x = block->LLX() + blk_pin_list[0].XOffset();
  for (size_t i=0; i<blk_pin_list.size(); i++) {
    block = blk_pin_list[i].GetBlock();
    if (max_x < block->LLX() + blk_pin_list[i].XOffset()) {
      max_x = block->LLX() + blk_pin_list[i].XOffset();
      max_pin_index = i;
    }
  }
  return max_pin_index;
}

int Net::MinPinX() {
  size_t min_pin_index = 0;
  auto *block = blk_pin_list[0].GetBlock();
  double min_x = block->LLX() + blk_pin_list[0].XOffset();
  for (size_t i=0; i<blk_pin_list.size(); i++) {
    block = blk_pin_list[i].GetBlock();
    if (min_x > block->LLX() + blk_pin_list[i].XOffset()) {
      min_x = block->LLX() + blk_pin_list[i].XOffset();
      min_pin_index = i;
    }
  }
  return min_pin_index;
}

int Net::MaxPinY() {
  size_t max_pin_index = 0;
  auto *block = blk_pin_list[0].GetBlock();
  double max_y = block->LLY() + blk_pin_list[0].YOffset();
  for (size_t i=0; i<blk_pin_list.size(); i++) {
    block = blk_pin_list[i].GetBlock();
    if (max_y < block->LLY() + blk_pin_list[i].YOffset()) {
      max_y = block->LLY() + blk_pin_list[i].YOffset();
      max_pin_index = i;
    }
  }
  return max_pin_index;
}

int Net::MinPinY() {
  size_t min_pin_index = 0;
  auto *block = blk_pin_list[0].GetBlock();
  double min_y = block->LLY() + blk_pin_list[0].YOffset();
  for (size_t i=0; i<blk_pin_list.size(); i++) {
    block = blk_pin_list[i].GetBlock();
    if (min_y < block->LLY() + blk_pin_list[i].YOffset()) {
      min_y = block->LLY() + blk_pin_list[i].YOffset();
      min_pin_index = i;
    }
  }
  return min_pin_index;
}

double Net::HPWLX() {
  if (blk_pin_list.empty()) {
    std::cout << "Error!\n";
    std::cout << "net contains no pin\n";
    assert(!blk_pin_list.empty());
  }
  auto *block = blk_pin_list[0].GetBlock();
  double max_x = block->LLX() + blk_pin_list[0].XOffset();
  double min_x = block->LLX() + blk_pin_list[0].YOffset();

  for (auto &&pin: blk_pin_list) {
    if (pin.GetBlock() == nullptr) {
      std::cout << "Error!\n";
      std::cout << "attribute block_t* _block is nullptr, it should points to the block containing this pin\n";
      assert(pin.GetBlock() != nullptr);
    }
    block = pin.GetBlock();
    if (max_x < block->LLX() + pin.XOffset()) {
      max_x = block->LLX() + pin.XOffset();
    }
    if (min_x > block->LLX() + pin.XOffset()) {
      min_x = block->LLX() + pin.XOffset();
    }
  }
  
  return (max_x - min_x);
}

double Net::HPWLY() {
  if (blk_pin_list.empty()) {
    std::cout << "Error!\n";
    std::cout << "net contains no pin\n";
    assert(!blk_pin_list.empty());
  }
  auto *block = blk_pin_list[0].GetBlock();
  double max_y = block->LLY() + blk_pin_list[0].YOffset();
  double min_y = block->LLY() + blk_pin_list[0].YOffset();

  for (auto &&pin: blk_pin_list) {
    if (pin.GetBlock() == nullptr) {
      std::cout << "Error!\n";
      std::cout << "attribute block_t* _block is nullptr, it should points to the block containing this pin\n";
      assert(pin.GetBlock() != nullptr);
    }
    block = pin.GetBlock();
    if (max_y < block->LLY() + pin.YOffset()) {
      max_y = block->LLY() + pin.YOffset();
    }
    if (min_y > block->LLY() + pin.YOffset()) {
      min_y = block->LLY() + pin.YOffset();
    }
  }

  return (max_y - min_y);
}

double Net::HPWL() {
  return HPWLX() + HPWLY();
}

