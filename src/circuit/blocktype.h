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
  const std::string *pname_;
  int width_, height_;
  long int area_;

 public:
  BlockTypeWell *ptr_well_;
  std::vector<Pin> pin_list;
  std::map<std::string, int> pin_name_num_map;

  BlockType(const std::string *name, int width, int height);

  void SetName(const std::string *pname) {
    assert(pname != nullptr);
    pname_ = pname;
  }
  const std::string *Name() const { return pname_; }

  bool IsPinExist(std::string &pin_name) {
    return pin_name_num_map.find(pin_name) != pin_name_num_map.end();
  }
  int PinIndex(std::string &pin_name) {
    Assert(IsPinExist(pin_name), "Pin does not exist, cannot find its index: " + pin_name);
    return pin_name_num_map.find(pin_name)->second;
  }

  Pin *AddPin(std::string &pin_name);
  void AddPin(std::string &pin_name, double x_offset, double y_offset);

  Pin *GetPin(std::string &pin_name);

  void SetWell(BlockTypeWell *ptr_well) {
    Assert(ptr_well != nullptr, "Cannot set well info to nullptr");
    ptr_well_ = ptr_well;
  }
  BlockTypeWell *GetWell() const { return ptr_well_; }

  void SetWidth(int width) {
    width_ = width;
    area_ = (long int) width_ * (long int) height_;
  }
  int Width() const { return width_; }

  void SetHeight(int height) {
    height_ = height;
    area_ = (long int) width_ * (long int) height_;
  }
  int Height() const { return height_; }

  long int Area() const { return area_; }
  bool Empty() const { return pin_list.empty(); }

  void Report() const;
};

#endif //DALI_SRC_CIRCUIT_BLOCKTYPE_H_
