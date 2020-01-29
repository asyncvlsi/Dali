//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef DALI_BLOCK_HPP
#define DALI_BLOCK_HPP

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
  unsigned int Width() const { return type_->Width(); }
  unsigned int Height() const { return type_->Height(); }
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
  void DecreX(double displacement) { llx_ -= displacement; }
  void DecreY(double displacement) { lly_ -= displacement; }
  bool IsOverlap(const Block &rhs) const {
    return !(LLX() > rhs.URX() || rhs.LLX() > URX() || LLY() > rhs.URY() || rhs.LLY() > URY());
  }
  bool IsOverlap(const Block *rhs) const { return IsOverlap(*rhs); }
  double OverlapArea(const Block &rhs) const;
  double OverlapArea(const Block *rhs) const { return OverlapArea(*rhs); }

  void Report();
  void ReportNet();

  const std::string *TypeName() const { return type_->Name(); }
  std::string GetPlaceStatusStr() { return PlaceStatusStr(place_status_); }
  std::string LowerLeftCorner() { return "( " + std::to_string(LLX()) + " " + std::to_string(LLY()) + " )"; }
};

class BlockAux {
 protected:
  Block *block_;
 public:
  explicit BlockAux(Block *block) : block_(block) { block->SetAux(this); }
  Block *GetBlock() { return block_; }
};

inline void Block::Report() {
  std::cout << "  block name: " << *Name() << "\n"
            << "    block type: " << *(Type()->Name()) << "\n"
            << "    width and height: " << Width() << " " << Height() << "\n"
            << "    lower left corner: " << llx_ << " " << lly_ << "\n"
            << "    movable: " << IsMovable() << "\n"
            << "    orientation: " << OrientStr(orient_) << "\n"
            << "    assigned primary key: " << Num()
            << "\n";
}

inline Block::Block(BlockType *type,
                    std::pair<const std::string, int> *name_num_pair,
                    int llx,
                    int lly,
                    bool movable,
                    BlockOrient orient) : type_(
    type), name_num_pair_(name_num_pair), llx_(llx), lly_(lly), orient_(orient) {
  Assert(name_num_pair != nullptr,
         "Must provide a valid pointer to the std::pair<std::string, int> element in the block_name_map");
  aux_ = nullptr;
  if (movable) {
    place_status_ = UNPLACED;
  } else {
    place_status_ = FIXED;
  }
}

inline Block::Block(BlockType *type,
                    std::pair<const std::string, int> *name_num_pair,
                    int llx,
                    int lly,
                    PlaceStatus place_state,
                    BlockOrient orient) :
    type_(type), name_num_pair_(name_num_pair), llx_(llx), lly_(lly), place_status_(place_state), orient_(orient) {
  Assert(name_num_pair != nullptr,
         "Must provide a valid pointer to the std::pair<std::string, int> element in the block_name_map");
  aux_ = nullptr;
}

inline void Block::IncreX(double displacement, double upper, double lower) {
  llx_ += displacement;
  double real_upper = upper - Width();
  if (llx_ < lower) {
    llx_ = lower;
  } else if (llx_ > real_upper) {
    llx_ = real_upper;
  }
}

inline void Block::IncreY(double displacement, double upper, double lower) {
  lly_ += displacement;
  double real_upper = upper - Height();
  if (lly_ < lower) {
    lly_ = lower;
  } else if (lly_ > real_upper) {
    lly_ = real_upper;
  }
}

inline double Block::OverlapArea(const Block &rhs) const {
  double overlap_area = 0;
  if (IsOverlap(rhs)) {
    double llx, urx, lly, ury;
    llx = std::max(LLX(), rhs.LLX());
    urx = std::min(URX(), rhs.URX());
    lly = std::max(LLY(), rhs.LLY());
    ury = std::min(URY(), rhs.URY());
    overlap_area = (urx - llx) * (ury - lly);
  }
  return overlap_area;
}

inline void Block::ReportNet() {
  std::cout << *Name() << " connects to:\n";
  for (auto &&net_num: net_list) {
    std::cout << net_num << "  ";
  }
  std::cout << "\n";
}

inline void Block::SwapLoc(Block &blk) {
  double tmp_x = llx_;
  double tmp_y = lly_;
  llx_ = blk.LLX();
  lly_ = blk.LLY();
  blk.SetLLX(tmp_x);
  blk.SetLLY(tmp_y);
}

#endif //DALI_BLOCK_HPP
