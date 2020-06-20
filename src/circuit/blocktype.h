//
// Created by Yihang Yang on 2019-10-31.
//

#ifndef DALI_SRC_CIRCUIT_BLOCKTYPE_H_
#define DALI_SRC_CIRCUIT_BLOCKTYPE_H_

#include <map>
#include <vector>

#include "common/misc.h"
#include "pin.h"

/****
 * The class BlockType is an abstraction of a kind of gate, like INV, NAND, C-element
 * Here are the attributes of BlockType:
 *  name: the name of this kind of gate
 *  width: the width of its boundary
 *  height: the height of its boundary
 *  well: this is for gridded cell placement, N/P-well shapes are needed for alignment in a local cluster
 *  pin: a list of cell pins with shapes and offsets specified
 * ****/

class Pin;
struct BlockTypeWell;

class BlockType {
  friend class Circuit;
 private:
  /****essential data entries****/
  const std::string *name_ptr_;
  int width_, height_;
  long int area_;
  BlockTypeWell *well_ptr_;
  std::vector<Pin> pin_list_;
  std::map<std::string, int> pin_name_num_map_;
 public:
  BlockType(const std::string *name_ptr, int width, int height);

  // set the name of this BlockType
  void setName(const std::string *name_ptr) {
    Assert(name_ptr != nullptr, "Cannot set @param name_ptr to nullptr in function: BlockType::SetName()\n");
    name_ptr_ = name_ptr;
  }

  // get the pointer to the const name
  const std::string *NamePtr() const { return name_ptr_; }

  // check if a pin with a given name exists in this BlockType or not
  bool IsPinExist(std::string &pin_name) {
    return pin_name_num_map_.find(pin_name) != pin_name_num_map_.end();
  }

  // return the index of a pin with a given name
  int PinIndex(std::string &pin_name) {
    Assert(IsPinExist(pin_name), "Pin does not exist, cannot find its index: " + pin_name);
    return pin_name_num_map_.find(pin_name)->second;
  }

  // return a pointer to a newly allocated location for a Pin with a given name
  // if this member function is used to create pins, one needs to set pin shapes using the return pointer
  Pin *AddPin(std::string &pin_name);

  // add a pin with a given name and x/y offset
  void AddPin(std::string &pin_name, double x_offset, double y_offset);

  // get the pointer to the pin with the given name
  // if such a pin does not exist, the return value is nullptr
  Pin *getPinPtr(std::string &pin_name) {
    auto res = pin_name_num_map_.find(pin_name);
    return res == pin_name_num_map_.end() ? nullptr : &(pin_list_[res->second]);
  }

  // set the N/P-well information for this BlockType
  void setWell(BlockTypeWell *well_ptr) {
    Assert(well_ptr != nullptr, "Cannot set @param well_ptr to nullptr in function: BlockType::SetWell()");
    well_ptr_ = well_ptr;
  }

  // get the pointer to the well of this BlockType
  BlockTypeWell *WellPtr() const { return well_ptr_; }

  // set the width of this BlockType and update its area
  void setWidth(int width) {
    width_ = width;
    area_ = (long int) width_ * (long int) height_;
  }

  // get the width of this BlockType
  int Width() const { return width_; }

  // set the height of this BlockType and update its area
  void setHeight(int height) {
    height_ = height;
    area_ = (long int) width_ * (long int) height_;
  }

  // get the height of this BlockType
  int Height() const { return height_; }

  // get the area of this BlockType
  long int Area() const { return area_; }

  // get the pointer to the list of cell pins
  std::vector<Pin> *PinList() { return &pin_list_; }

  // check if the pin_list_ is empty
  bool Empty() const { return pin_list_.empty(); }

  // report the information of this BlockType for debugging purposes
  void Report() const;
};

#endif //DALI_SRC_CIRCUIT_BLOCKTYPE_H_
