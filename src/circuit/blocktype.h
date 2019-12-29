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

  void SetName(const std::string *name) {name_ = name;}
  bool PinExist(std::string &pin_name);
  int PinIndex(std::string &pin_name);
  Pin *AddPin(std::string &pin_name);
  void AddPin(std::string &pin_name, double x_offset, double y_offset);

  void SetWell(BlockTypeWell *well);
  BlockTypeWell *GetWell() const {return well_;}

  const std::string *Name() const { return name_;}
  unsigned int Width() const {return width_;}
  unsigned int Height() const {return height_;}
  unsigned long int Area() const {return width_ * height_;}
  void SetWidth(unsigned int width) {width_ = width;}
  void SetHeight(unsigned int height) {height_ = height;}
  bool Empty() const {return pin_list.empty();}

  void Report() const;
};

#endif //DALI_SRC_CIRCUIT_BLOCKTYPE_H_
