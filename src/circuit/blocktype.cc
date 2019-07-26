//
// Created by Yihang Yang on 2019-06-27.
//

#include "blocktype.h"

BlockType::BlockType(std::string &name, int width, int height, int num) : name_(name), width_(width), height_(height), num_(num) {}

const std::string *BlockType::Name() const {
  return &name_;
}

int BlockType::Num() const {
  return  num_;
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

void BlockType::SetName(std::string &typeName) {
  Assert(!typeName.empty(), "Empty block type Name?");
  name_ = typeName;
}

void BlockType::SetWidth(int width) {
  Assert(width > 0, "Width equal or less than 0?");
  width_ = width;
}

void BlockType::SetHeight(int height) {
  Assert(height > 0, "Height equal or less than 0?");
  height_ = height;
}

void BlockType::AddPin(std::string &pin_name, double x_offset, double y_offset) {
  bool pin_not_exist = pin_name_num_map.find(pin_name) == pin_name_num_map.end();
  Assert(pin_not_exist, "The following pin exists in pin_list: " + pin_name);
  pin_name_num_map.insert(std::pair<std::string, size_t>(pin_name, pin_list.size()));
  pin_list.emplace_back(x_offset, y_offset);
}