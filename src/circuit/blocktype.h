//
// Created by Yihang Yang on 10/31/19.
//

#ifndef HPCC_SRC_CIRCUIT_BLOCKTYPE_H_
#define HPCC_SRC_CIRCUIT_BLOCKTYPE_H_

#include "pin.h"
#include <map>
#include <vector>

class BlockType {
 private:
  /****essential data entries****/
  //std::pair<const std::string, int>* name_num_pair_ptr_;
  const std::string *name_;
  int width_, height_;
 public:
  BlockType(int width, int height);
  void SetName(const std::string *name) {name_ = name;}
  std::vector<Pin> pin_list;
  std::map<std::string, int> pin_name_num_map;

  bool PinExist(std::string &pin_name);
  int PinIndex(std::string &pin_name);
  Pin *AddPin(std::string &pin_name);
  void AddPin(std::string &pin_name, double x_offset, double y_offset);

  void AddWell(bool is_pluged, int llx, int lly, int urx, int ury);

  const std::string *Name() const { return name_;}
  int Width() const;
  int Height() const;
  int Area() const;
  void SetWidth(int width);
  void SetHeight(int height);
  bool Empty();

  void Report();

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
