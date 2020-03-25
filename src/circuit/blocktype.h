//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_SRC_CIRCUIT_BLOCKTYPE_H_
#define DALI_SRC_CIRCUIT_BLOCKTYPE_H_

#include <map>
#include <vector>

#include "pin.h"
#include "blocktypewell.h"

class Pin;
class BlockTypeWell;

class BlockType {
 private:
  /****essential data entries****/
  const std::string *name_;
  int width_, height_;
  long int area_;

 public:
  BlockTypeWell *well_;
  std::vector<Pin> pin_list;
  std::map<std::string, int> pin_name_num_map;

  BlockType(const std::string *name, int width, int height);

  void SetName(const std::string *name);
  const std::string *Name() const;

  bool IsPinExist(std::string &pin_name);
  int PinIndex(std::string &pin_name);

  Pin *AddPin(std::string &pin_name);
  void AddPin(std::string &pin_name, double x_offset, double y_offset);

  Pin *GetPin(std::string &pin_name);

  void SetWell(BlockTypeWell *well);
  BlockTypeWell *GetWell() const;

  void SetWidth(int width);
  int Width() const;

  void SetHeight(int height);
  int Height() const;

  long int Area() const;
  bool Empty() const;

  void Report() const;
};

inline void BlockType::SetName(const std::string *name) {
  assert(name != nullptr);
  name_ = name;
}

inline const std::string *BlockType::Name() const {
  return name_;
}

inline bool BlockType::IsPinExist(std::string &pin_name) {
  return pin_name_num_map.find(pin_name) != pin_name_num_map.end();
}

inline int BlockType::PinIndex(std::string &pin_name) {
  Assert(IsPinExist(pin_name), "Pin does not exist, cannot find its index: " + pin_name);
  return pin_name_num_map.find(pin_name)->second;
}

inline void BlockType::SetWell(BlockTypeWell *well) {
  Assert(well != nullptr, "When set auxiliary information, argument cannot be a nullptr");
  well_ = well;
}

inline BlockTypeWell *BlockType::GetWell() const {
  return well_;
}

inline void BlockType::SetWidth(int width) {
  width_ = width;
  area_ = width_ * height_;
}

inline int BlockType::Width() const {
  return width_;
}

inline void BlockType::SetHeight(int height) {
  height_ = height;
  area_ = width_ * height_;
}

inline int BlockType::Height() const {
  return height_;
}

inline long int BlockType::Area() const {
  return area_;
}

inline bool BlockType::Empty() const {
  return pin_list.empty();
}

#endif //DALI_SRC_CIRCUIT_BLOCKTYPE_H_
