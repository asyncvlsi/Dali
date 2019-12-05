//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_BLOCK_HPP
#define HPCC_BLOCK_HPP


#include <vector>
#include <map>
#include <string>
#include <iostream>
#include "status.h"
#include "blockaux.h"
#include "blocktype.h"
#include "common/misc.h"

/****
 * a block can be a gate, can also be a large module, it includes information like
 * the Name of a gate/module, its Width and Height, its lower Left corner (LLX, LLY),
 * the movability, Orient.
 *
 * lefdefref version 5.8, page 129
 * After placement, a DEF COMPONENTS placement pt indicates
 * where the lower-left corner of the placement bounding
 * rectangle is placed after any possible rotations or flips. The
 * bounding rectangle width and height should be a multiple of
 * the placement grid to allow for abutting cells.
 * ****/

class BlockAux;

class Block {
 protected:
  /* essential data entries */
  BlockType *type_;
  std::pair<const std::string, int>* name_num_pair_;
  double llx_, lly_; // lower Left corner
  PlaceStatus place_status_;
  BlockOrient orient_;
  BlockAux *aux_;
 public:
  bool is_placed = false;
  Block(BlockType *type, std::pair<const std::string, int>* name_num_pair, int llx, int lly, bool movable = "true", BlockOrient orient = N);
  Block(BlockType *type, std::pair<const std::string, int>* name_num_pair, int llx, int lly, PlaceStatus place_state = UNPLACED, BlockOrient orient = N);
  std::vector<int> net_list;

  const std::string *Name() const {return &(name_num_pair_->first);}
  BlockType *Type() const {return type_;}
  int Num() const {return name_num_pair_->second;}
  int Width() const {return type_->Width();}
  int Height() const {return type_->Height();}
  double LLX() const;
  double LLY() const;
  double URX() const;
  double URY() const;
  double X() const;
  double Y() const;
  PlaceStatus GetPlaceStatus() {return place_status_;}
  bool IsMovable() const;
  bool IsFixed() const;
  unsigned long int Area() const {return type_->Area();}
  BlockOrient Orient() const {return orient_;}
  BlockAux *Aux() {return aux_;}

  void SetLLX(double lower_left_x);
  void SetLLY(double lower_left_y);
  void SetURX(double upper_right_x);
  void SetURY(double upper_right_y);
  void SetCenterX(double center_x);
  void SetCenterY(double center_y);
  void SetPlaceStatus(PlaceStatus place_status) {place_status_ = place_status;}
  void SetOrient(BlockOrient &orient);
  void SetAux(BlockAux *aux);
  void SwapLoc(Block &blk);

  void IncreX(double displacement);
  void IncreY(double displacement);
  void IncreX(double displacement, double upper, double lower);
  void IncreY(double displacement, double upper, double lower);
  bool IsOverlap(const Block &rhs) const;
  bool IsOverlap(const Block *rhs) const {return IsOverlap(*rhs);}
  double OverlapArea(const Block &rhs) const;
  double OverlapArea(const Block *rhs) const;

  void Report();
  void ReportNet();

  /*friend std::ostream& operator<<(std::ostream& os, const Block &block) {
    os << "Block Name: " << *block.Name() << "\n";
    os << "Block Type: " << *(block.Type()->Name()) << "\n";
    os << "Width and Height: " << block.Width() << " " << block.Height() << "\n";
    os << "lower Left corner: " << block.LLX() << " " << block.LLY() << "\n";
    os << "movability: " << block.IsMovable() << "\n";
    os << "orientation: " << block.OrientStr() << "\n";
    os << "assigned primary key: " << block.Num() << "\n";
    return os;
  }*/

  const std::string *TypeName() const {return type_->Name();}
  std::string GetPlaceStatusStr() {return PlaceStatusStr(place_status_);}
  std::string LowerLeftCorner() {return "( " + std::to_string(LLX()) + " " + std::to_string(LLY()) + " )";}
};

#endif //HPCC_BLOCK_HPP
