//
// Created by Yihang Yang on 10/31/19.
//

#include "blocktype.h"

#include "common/misc.h"

BlockType::BlockType(const std::string *name, unsigned int width, unsigned int height)
    : name_(name), width_(width), height_(height), well_(nullptr) {}

void BlockType::Report() const {
  std::cout << "  BlockType name: " << *Name() << "\n"
            << "    width, height: " << Width() << " " << Height() << "\n"
            << "    pin list:\n";
  for (auto &&it: pin_name_num_map) {
    std::cout << "      " << it.first << " " << it.second << " (" << pin_list[it.second].XOffset() << ", "
              << pin_list[it.second].YOffset() << ")\n";
  }
  std::cout << std::endl;
}

bool BlockType::PinExist(std::string &pin_name) {
  return !(pin_name_num_map.find(pin_name) == pin_name_num_map.end());
}

int BlockType::PinIndex(std::string &pin_name) {
  Assert(PinExist(pin_name), "Pin does not exist, cannot find its index: " + pin_name);
  return pin_name_num_map.find(pin_name)->second;
}

Pin *BlockType::AddPin(std::string &pin_name) {
  bool pin_not_exist = (pin_name_num_map.find(pin_name) == pin_name_num_map.end());
  Assert(pin_not_exist,
         "Cannot add this pin in BlockType: " + *Name() + ", because this pin exists in blk_pin_list: " + pin_name);
  pin_name_num_map.insert(std::pair<std::string, int>(pin_name, pin_list.size()));
  std::pair<const std::string, int> *name_num_ptr = &(*pin_name_num_map.find(pin_name));
  pin_list.emplace_back(name_num_ptr, this);
  return &pin_list.back();
}

void BlockType::AddPin(std::string &pin_name, double x_offset, double y_offset) {
  bool pin_not_exist = pin_name_num_map.find(pin_name) == pin_name_num_map.end();
  Assert(pin_not_exist, "Cannot add this pin, because this pin exists in blk_pin_list: " + pin_name);
  pin_name_num_map.insert(std::pair<std::string, int>(pin_name, pin_list.size()));
  std::pair<const std::string, int> *name_num_ptr = &(*pin_name_num_map.find(pin_name));
  pin_list.emplace_back(name_num_ptr, this, x_offset, y_offset);
}

void BlockType::SetWell(BlockTypeWell *well) {
  Assert(well != nullptr, "When set auxiliary information, argument cannot be a nullptr");
  well_ = well;
}
