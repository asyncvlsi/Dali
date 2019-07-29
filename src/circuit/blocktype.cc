//
// Created by Yihang Yang on 2019-06-27.
//

#include "blocktype.h"

BlockType::BlockType(STR_INT_PAIR_PTR name_num_pair_ptr, int width, int height) : name_num_pair_ptr_(name_num_pair_ptr), width_(width), height_(height) {

}

const std::string *BlockType::Name() const {
  return &(name_num_pair_ptr_->first);
}

int BlockType::Num() const {
  return name_num_pair_ptr_->second;
}

int BlockType::Width() const {
  return width_;
}

int BlockType::Height() const {
  return height_;
}

int BlockType::Area() const {
  return width_ * height_;
}

void BlockType::SetWidth(int width) {
  Assert(width > 0, "Width equal or less than 0?");
  width_ = width;
}

void BlockType::SetHeight(int height) {
  Assert(height > 0, "Height equal or less than 0?");
  height_ = height;
}

bool BlockType::PinExist(std::string &pin_name) {
  return !(pin_name.find(block_type_name) == block_type_name_map.end());
}

int BlockType::PinIndex(std::string &pin_name) {

}

Pin *BlockType::AddPin(std::string &pin_name) {
  bool pin_not_exist = pin_name_num_map.find(pin_name) == pin_name_num_map.end();
  Assert(pin_not_exist, "The following pin exists in pin_list: " + pin_name);
  pin_name_num_map.insert(std::pair<std::string, size_t>(pin_name, pin_list.size()));
  pin_list.emplace_back(pin_name);
  return &pin_list.back();
}