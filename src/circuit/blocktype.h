//
// Created by Yihang Yang on 10/31/19.
//

#ifndef HPCC_SRC_CIRCUIT_BLOCKTYPE_H_
#define HPCC_SRC_CIRCUIT_BLOCKTYPE_H_

#include <map>
#include <vector>
#include "pin.h"
#include "blocktypeaux.h"

class Pin;

class BlockType {
 private:
  /****essential data entries****/
  const std::string *name_;
  unsigned int width_, height_;

  BlockTypeAux *aux_;
 public:
  BlockType(const std::string *name, unsigned int width, unsigned int height);
  void SetName(const std::string *name) {name_ = name;}
  std::vector<Pin> pin_list;
  std::map<std::string, int> pin_name_num_map;

  bool PinExist(std::string &pin_name);
  int PinIndex(std::string &pin_name);
  Pin *AddPin(std::string &pin_name);
  void AddPin(std::string &pin_name, double x_offset, double y_offset);

  void SetAux(BlockTypeAux *aux);
  BlockTypeAux *GetAux() const {return aux_;}

  const std::string *Name() const { return name_;}
  unsigned int Width() const {return width_;}
  unsigned int Height() const {return height_;}
  unsigned long int Area() const {return width_ * height_;}
  void SetWidth(unsigned int width) {width_ = width;}
  void SetHeight(unsigned int height) {height_ = height;}
  bool Empty() const {return pin_list.empty();}

  void Report() const;

  /*friend std::ostream& operator<<(std::ostream& os, const BlockType &block_type) {
    os << "BlockType name: " << *block_type.Name() << "\n";
    os << "  width, height: " << block_type.Width() << " " << block_type.Height() << "\n";
    os << "  assigned key: " << block_type.Num() << "\n";
    os << "  pin list:\n";
    for( auto &&it: block_type.pin_name_num_map) {
      os << "    " << it.first << " " << it.second << " (" << block_type.pin_list[it.second].XOffset() << ", " << block_type.pin_list[it.second].YOffset() << ")\n";
    }
    return os;
  }*/
};

#endif //HPCC_SRC_CIRCUIT_BLOCKTYPE_H_
