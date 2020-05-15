//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef DALI_BLOCK_H
#define DALI_BLOCK_H

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "blocktype.h"
#include "common/misc.h"
#include "status.h"

/****
 * a block can be a gate, can also be a large module, it includes information like
 * the Name of a gate/module, its Width and Height, its lower left corner (LLX, LLY),
 * the movability, orientation.
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
  /**** essential data entries ***/
  BlockType *type_; // type
  int eff_height_; // cached height, also used to store effective height
  long int eff_area_; // cached effective area
  std::pair<const std::string, int> *name_num_pair_; // name for finding its index in block_list
  double llx_; // lower x coordinate
  double lly_; // lower y coordinate
  PlaceStatus place_status_; // placement status, i.e, PLACED, FIXED, UNPLACED
  BlockOrient orient_; // orientation, normally, N or FS
  BlockAux *aux_; // points to auxiliary information if needed
 public:
  /****this constructor is for the developer only****/
  Block();

  /****Constructors for users****/
  Block(BlockType *type,
        std::pair<const std::string, int> *name_num_pair,
        int llx,
        int lly,
        bool movable = "true",
        BlockOrient orient = N_);
  Block(BlockType *type,
        std::pair<const std::string, int> *name_num_pair,
        int llx,
        int lly,
        PlaceStatus place_state = UNPLACED_,
        BlockOrient orient = N_);

  std::vector<int> net_list;

  /****member functions for attributes access****/
  const std::string *Name() const { return &(name_num_pair_->first); }
  std::string NameStr() const { return std::string(name_num_pair_->first); }
  BlockType *Type() const { return type_; }
  int Num() const { return name_num_pair_->second; }
  int Width() const { return type_->Width(); }
  void SetHeight(int height) {
    eff_height_ = height;
    eff_area_ = eff_height_ * type_->Width();
  }
  void SetHeightFromType() {
    eff_height_ = type_->Height();
    eff_area_ = type_->Area();
  }
  int Height() const { return eff_height_; }
  double LLX() const { return llx_; }
  double LLY() const { return lly_; }
  double URX() const { return llx_ + Width(); }
  double URY() const { return lly_ + Height(); }
  double X() const { return llx_ + Width() / 2.0; }
  double Y() const { return lly_ + Height() / 2.0; }
  bool IsPlaced() const { return place_status_ == PLACED_ || place_status_ == FIXED_ || place_status_ == COVER_; }
  PlaceStatus GetPlaceStatus() { return place_status_; }
  bool IsMovable() const { return place_status_ == UNPLACED_ || place_status_ == PLACED_; }
  bool IsFixed() const { return !IsMovable(); }
  long int Area() const { return eff_area_; }
  BlockOrient Orient() const { return orient_; }
  BlockAux *Aux() const { return aux_; }

  void SetNameNumPair(std::pair<const std::string, int> *name_num_pair) { name_num_pair_ = name_num_pair; }
  void SetType(BlockType *type) {
    Assert(type != nullptr, "Cannot set BlockType of a Block to NULL");
    type_ = type;
    eff_height_ = type_->Height();
    eff_area_ = type_->Area();
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
  void SetOrient(BlockOrient const &orient) { orient_ = orient; }
  void SetAux(BlockAux *aux) {
    Assert(aux != nullptr, "When set auxiliary information, argument cannot be a nullptr");
    aux_ = aux;
  }

  void SwapLoc(Block &blk);

  void IncreaseX(double displacement) { llx_ += displacement; }
  void IncreaseY(double displacement) { lly_ += displacement; }
  void IncreaseX(double displacement, double upper, double lower);
  void IncreaseY(double displacement, double upper, double lower);
  void DecreaseX(double displacement) { llx_ -= displacement; }
  void DecreaseY(double displacement) { lly_ -= displacement; }
  bool IsOverlap(const Block &rhs) const {
    return !(LLX() > rhs.URX() || rhs.LLX() > URX() || LLY() > rhs.URY() || rhs.LLY() > URY());
  }
  bool IsOverlap(const Block *rhs) const { return IsOverlap(*rhs); }
  double OverlapArea(const Block &rhs) const;
  double OverlapArea(const Block *rhs) const { return OverlapArea(*rhs); }

  /****Report info in this block, for debugging****/
  void Report();
  void ReportNet();

  std::string GetPlaceStatusStr() { return PlaceStatusStr(place_status_); }
  std::string LowerLeftCorner() { return "( " + std::to_string(LLX()) + " " + std::to_string(LLY()) + " )"; }
};

class BlockAux {
 protected:
  Block *block_;
 public:
  explicit BlockAux(Block *block) {block->SetAux(this);}
  Block *GetBlock() { return block_; }
};

#endif //DALI_BLOCK_H
