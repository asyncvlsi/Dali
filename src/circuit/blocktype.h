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
  unsigned int width_, height_;

 public:
  BlockTypeWell *well_;
  std::vector<Pin> pin_list;
  std::map<std::string, int> pin_name_num_map;

  BlockType(const std::string *name, unsigned int width, unsigned int height);

  void SetName(const std::string *name);
  const std::string *Name() const;

  bool IsPinExist(std::string &pin_name);
  int PinIndex(std::string &pin_name);

  Pin *AddPin(std::string &pin_name);
  void AddPin(std::string &pin_name, double x_offset, double y_offset);

  Pin *GetPin(std::string &pin_name);

  void SetWell(BlockTypeWell *well);
  BlockTypeWell *GetWell() const;

  unsigned int Width() const;
  unsigned int Height() const;
  unsigned long int Area() const;
  void SetWidth(unsigned int width);
  void SetHeight(unsigned int height);
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

inline unsigned int BlockType::Width() const {
  return width_;
}

inline unsigned int BlockType::Height() const {
  return height_;
}

inline unsigned long int BlockType::Area() const {
  return width_ * height_;
}

inline void BlockType::SetWidth(unsigned int width) {
  width_ = width;
}

inline void BlockType::SetHeight(unsigned int height) {
  height_ = height;
}

inline bool BlockType::Empty() const {
  return pin_list.empty();
}

#endif //DALI_SRC_CIRCUIT_BLOCKTYPE_H_
