//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_BLOCK_HPP
#define HPCC_BLOCK_HPP


#include <vector>
#include <string>
#include <iostream>
#include "blocktype.h"
#include "blockaux.h"
#include "common/misc.h"

/* a block can be a gate, can also be a large module, it includes information like
 * the Name of a gate/module, its Width and Height, its lower Left corner (LLX, LLY),
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

class BlockAux;

class Block {
 protected:
  /* essential data entries */
  BlockType *type_;
  std::pair<const std::string, int>* name_num_pair_ptr_;
  double llx_, lly_; // lower Left corner
  bool movable_; // movable
  enum BlockOrient orient_; // currently not used
  BlockAux *aux_;
 public:
  Block(BlockType *type, std::pair<const std::string, int>* name_num_pair, int llx, int lly, bool movable = "true", BlockOrient orient = N);
  const std::string *Name() const;
  BlockType *Type() const;
  int Num() const;
  int Width() const;
  int Height() const;
  void SetLLX(double lower_left_x);
  double LLX() const;
  void SetLLY(double lower_left_y);
  double LLY() const;
  void SetURX(double upper_right_x);
  double URX() const;
  void SetURY(double upper_right_y);
  double URY() const;
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
  void IncreX(double amount);
  void IncreY(double amount);
  bool is_overlap(const Block &rhs) const;
  void SetAux(BlockAux *aux);
  BlockAux *Aux();

  friend std::ostream& operator<<(std::ostream& os, const Block &block) {
    os << "Block Name: " << *block.Name() << "\n";
    os << "Block Type: " << *(block.Type()->Name()) << "\n";
    os << "Width and Height: " << block.Width() << " " << block.Height() << "\n";
    os << "lower Left corner: " << block.LLX() << " " << block.LLY() << "\n";
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
