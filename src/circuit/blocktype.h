//
// Created by Yihang Yang on 2019-06-27.
//

#ifndef HPCC_BLOCKTYPE_HPP
#define HPCC_BLOCKTYPE_HPP

#include <string>
#include <map>
#include "pin.h"
#include "common/misc.h"

class BlockType {
private:
  /****essential data entries****/
  std::string name_;
  std::map<std::string,int>::iterator name_num_pair_ptr_;
  int width_, height_;

  /****cached data entries****/
  int num_;
public:
  BlockType(std::string &name, int width, int height, int num = 0);

  /****essential data entries****/
  std::vector<Pin> pin_list;
  std::map<std::string, size_t> pin_name_num_map;
  /********/

  const std::string *Name() const;
  int Num() const;
  int Width() const;
  int Height() const;
  int Area() const;

  void SetName(std::string &typeName);
  void SetWidth(int width);
  void SetHeight(int height);

  bool PinExist(std::string &pin_name);
  int PinIndex(std::string &pin_name);
  Pin *NewPin(std::string &pin_name);

  friend std::ostream& operator<<(std::ostream& os, const BlockType &block_type) {
    os << "block type Name: " << block_type.name_ << "\n";
    os << "width and height: " << block_type.width_ << " " << block_type.height_ << "\n";
    os << "assigned primary key: " << block_type.num_ << "\n";
    os << "pin list:\n";
    for( auto &&it: block_type.pin_name_num_map) {
      os << "  " << it.first << " " << it.second << "  " << block_type.pin_list[it.second].XOffset() << " " << block_type.pin_list[it.second].YOffset() << "\n";
    }
    return os;
  }
};


#endif //HPCC_BLOCKTYPE_HPP
