//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef DALI_BLOCK_HPP
#define DALI_BLOCK_HPP

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "blockaux.h"
#include "blocktype.h"
#include "common/misc.h"
#include "status.h"

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
  std::pair<const std::string, int> *name_num_pair_;
  double llx_, lly_; // lower Left corner
  PlaceStatus place_status_;
  BlockOrient orient_;
  BlockAux *aux_;
 public:
  Block(BlockType *type,
        std::pair<const std::string, int> *name_num_pair,
        int llx,
        int lly,
        bool movable = "true",
        BlockOrient orient = N);
  Block(BlockType *type,
        std::pair<const std::string, int> *name_num_pair,
        int llx,
        int lly,
        PlaceStatus place_state = UNPLACED,
        BlockOrient orient = N);
  std::vector<int> net_list;

  const std::string *Name() const { return &(name_num_pair_->first); }
  BlockType *Type() const { return type_; }
  int Num() const { return name_num_pair_->second; }
  int Width() const { return type_->Width(); }
  int Height() const { return type_->Height(); }
  double LLX() const { return llx_; }
  double LLY() const { return lly_; }
  double URX() const { return llx_ + Width(); }
  double URY() const { return lly_ + Height(); }
  double X() const { return llx_ + Width() / 2.0; }
  double Y() const { return lly_ + Height() / 2.0; }
  bool IsPlaced() const { return place_status_ == PLACED || place_status_ == FIXED; }
  PlaceStatus GetPlaceStatus() { return place_status_; }
  bool IsMovable() const { return place_status_ == UNPLACED || place_status_ == PLACED; }
  bool IsFixed() const { return !IsMovable(); }
  unsigned long int Area() const { return type_->Area(); }
  BlockOrient Orient() const { return orient_; }
  BlockAux *Aux() const { return aux_; }

  void SetType(BlockType *type) {
    Assert(type != nullptr, "Cannot set BlockType of a Block to NULL");
    type_ = type;
  }
  void SetLoc(double lx, double ly) {
    llx_ = lx;
    lly_ = ly;
  }
  void SetLLX(double lx) { llx_ = lx; }
  void SetLLY(double ly) { lly_ = ly; }
  void SetURX(double ux) { llx_ = ux - Width(); }
  void SetURY(double uy) { lly_ = uy - Height(); }
  void SetCenterX(double center_x) { llx_ = center_x - Width() / 2.0; }
  void SetCenterY(double center_y) { lly_ = center_y - Height() / 2.0; }
  void SetPlaceStatus(PlaceStatus place_status) { place_status_ = place_status; }
  void SetOrient(BlockOrient &orient) { orient_ = orient; }
  void SetAux(BlockAux *aux) {
    Assert(aux != nullptr, "When set auxiliary information, argument cannot be a nullptr");
    aux_ = aux;
  }
  void SwapLoc(Block &blk);

  void IncreX(double displacement) { llx_ += displacement; }
  void IncreY(double displacement) { lly_ += displacement; }
  void IncreX(double displacement, double upper, double lower);
  void IncreY(double displacement, double upper, double lower);
  bool IsOverlap(const Block &rhs) const {
    return !(LLX() > rhs.URX() || rhs.LLX() > URX() || LLY() > rhs.URY() || rhs.LLY() > URY());
  }
  bool IsOverlap(const Block *rhs) const { return IsOverlap(*rhs); }
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

  const std::string *TypeName() const { return type_->Name(); }
  std::string GetPlaceStatusStr() { return PlaceStatusStr(place_status_); }
  std::string LowerLeftCorner() { return "( " + std::to_string(LLX()) + " " + std::to_string(LLY()) + " )"; }
};

#endif //DALI_BLOCK_HPP
