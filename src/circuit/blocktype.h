//
// Created by Yihang Yang on 2019-06-27.
//

#ifndef HPCC_BLOCKTYPE_HPP
#define HPCC_BLOCKTYPE_HPP

#include <string>
#include <map>
#include "pin.h"

class BlockType {
 private:
  /****essential data entries****/
  std::pair<const std::string, int>* name_num_pair_ptr_;
  int width_, height_;
 public:
  BlockType(std::pair<const std::string, int>* name_num_pair_ptr, int width, int height);
  std::vector<Pin> pin_list;
  std::map<std::string, int> pin_name_num_map;

  bool PinExist(std::string &pin_name);
  int PinIndex(std::string &pin_name);
  Pin *AddPin(std::string &pin_name);
  void AddPin(std::string &pin_name, double x_offset, double y_offset);

  void AddWell(bool is_pluged, int llx, int lly, int urx, int ury);

  const std::string *Name() const;
  int Num() const;
  int Width() const;
  int Height() const;
  int Area() const;
  void SetWidth(int width);
  void SetHeight(int height);
  bool Empty();

  friend std::ostream& operator<<(std::ostream& os, const BlockType &block_type) {
    os << "BlockType name: " << *block_type.Name() << "\n";
    os << "  width, height: " << block_type.Width() << " " << block_type.Height() << "\n";
    os << "  assigned key: " << block_type.Num() << "\n";
    os << "  pin list:\n";
    for( auto &&it: block_type.pin_name_num_map) {
      os << "    " << it.first << " " << it.second << " (" << block_type.pin_list[it.second].XOffset() << ", " << block_type.pin_list[it.second].YOffset() << ")\n";
    }
    return os;
  }
};


#endif //HPCC_BLOCKTYPE_HPP
