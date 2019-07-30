//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_BLOCK_HPP
#define HPCC_BLOCK_HPP


#include <vector>
#include <string>
#include <iostream>
#include "blocktype.h"
#include "common/misc.h"

/* a block can be a gate, can also be a large module, it includes information like
 * the Name of a gate/module, its Width and Height, its lower left corner (LLX, LLY),
 * the movability, Orient. */

enum BlockOrient{
  N = 0,
  S = 1,
  W = 2,
  E = 3,
  FN = 4,
  FS = 5,
  FW = 6,
  FE = 7
};

class Block {
protected:
  /* essential data entries */
  BlockType *type_;
  std::pair<const std::string, int>* name_num_pair_ptr_;
  int llx_, lly_; // lower left corner
  bool movable_; // movable
  enum BlockOrient orient_; // currently not used
public:
  Block(BlockType *type, std::pair<const std::string, int>* name_num_pair, int llx, int lly, bool movable = "true", BlockOrient orient = N);
  const std::string *Name() const;
  BlockType *Type() const;
  int Num() const;
  int Width() const;
  int Height() const;
  void SetLLX(int lower_left_x);
  int LLX() const;
  void SetLLY(int lower_left_y);
  int LLY() const;
  void SetURX(int upper_right_x);
  int URX() const;
  void SetURY(int upper_right_y);
  int URY() const;
  void SetCenterX(double center_x);
  double X() const;
  void SetCenterY(double center_y);
  double Y() const;
  void SetMovable(bool movable);
  bool IsMovable() const;
  int Area() const;
  void SetOrient(BlockOrient &orient);
  BlockOrient Orient() const;
  std::string OrientStr() const;

  friend std::ostream& operator<<(std::ostream& os, const Block &block) {
    os << "block Name: " << *block.Name() << "\n";
    os << "Width and Height: " << block.Width() << " " << block.Height() << "\n";
    os << "lower left corner: " << block.LLX() << " " << block.LLY() << "\n";
    os << "movability: " << block.IsMovable() << "\n";
    os << "orientation: " << block.OrientStr() << "\n";
    os << "assigned primary key: " << block.Num() << "\n";
    return os;
  }

  const std::string *TypeName() const;
  std::string IsPlace();
  std::string LowerLeftCorner();
};


#endif //HPCC_BLOCK_HPP
