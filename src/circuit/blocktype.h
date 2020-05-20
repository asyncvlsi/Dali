//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_SRC_CIRCUIT_BLOCKTYPE_H_
#define DALI_SRC_CIRCUIT_BLOCKTYPE_H_

#include <map>
#include <vector>

#include "common/misc.h"
#include "pin.h"

class Pin;
struct BlockTypeWell;

class BlockType {
 private:
  /****essential data entries****/
  const std::string *name_ptr_;
  int width_, height_;
  long int area_;

 public:
  BlockTypeWell *well_ptr_;
  std::vector<Pin> pin_list;
  std::map<std::string, int> pin_name_num_map;

  BlockType(const std::string *name_ptr, int width, int height);

  void SetName(const std::string *name_ptr) {
    Assert(name_ptr != nullptr, "Cannot set @param name_ptr to nullptr in function: BlockType::SetName()\n");
    name_ptr_ = name_ptr;
  }
  const std::string *Name() const { return name_ptr_; }

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

  void SetWell(BlockTypeWell *well_ptr) {
    Assert(well_ptr != nullptr, "Cannot set @param well_ptr to nullptr in function: BlockType::SetWell()");
    well_ptr_ = well_ptr;
  }
  BlockTypeWell *GetWell() const { return well_ptr_; }

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
