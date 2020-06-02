//
// Created by Yihang Yang on 2019-10-31.
//

#include "blocktype.h"

#include "common/misc.h"

BlockType::BlockType(const std::string *name_ptr, int width, int height)
    : name_ptr_(name_ptr),
      width_(width),
      height_(height),
      area_((long int) width_ * (long int) height_),
      well_ptr_(nullptr) {}

void BlockType::Report() const {
  std::cout << "  BlockType name: " << *NamePtr() << "\n"
            << "    width, height: " << Width() << " " << Height() << "\n"
            << "    pin list:\n";
  for (auto &it: pin_name_num_map_) {
    std::cout << "      " << it.first << " " << it.second << " (" << pin_list_[it.second].OffsetX() << ", "
              << pin_list_[it.second].OffsetY() << ")\n";
  }
  std::cout << std::endl;
}

Pin *BlockType::AddPin(std::string &pin_name) {
  bool pin_not_exist = (pin_name_num_map_.find(pin_name) == pin_name_num_map_.end());
  Assert(pin_not_exist,
         "Cannot add this pin in BlockType: " + *NamePtr() + ", because this pin exists in blk_pin_list: " + pin_name);
  pin_name_num_map_.insert(std::pair<std::string, int>(pin_name, pin_list_.size()));
  std::pair<const std::string, int> *name_num_ptr = &(*pin_name_num_map_.find(pin_name));
  pin_list_.emplace_back(name_num_ptr, this);
  return &pin_list_.back();
}

void BlockType::AddPin(std::string &pin_name, double x_offset, double y_offset) {
  bool pin_not_exist = pin_name_num_map_.find(pin_name) == pin_name_num_map_.end();
  Assert(pin_not_exist, "Cannot add this pin, because this pin exists in blk_pin_list: " + pin_name);
  pin_name_num_map_.insert(std::pair<std::string, int>(pin_name, pin_list_.size()));
  std::pair<const std::string, int> *name_num_ptr = &(*pin_name_num_map_.find(pin_name));
  pin_list_.emplace_back(name_num_ptr, this, x_offset, y_offset);
}
