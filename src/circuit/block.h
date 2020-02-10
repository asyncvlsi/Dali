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

  const std::string *Name() const;
  std::string NameStr() const;
  BlockType *Type() const;
  int Num() const;
  unsigned int Width() const;
  unsigned int Height() const;
  double LLX() const;
  double LLY() const;
  double URX() const;
  double URY() const;
  double X() const;
  double Y() const;
  bool IsPlaced() const;
  PlaceStatus GetPlaceStatus();
  bool IsMovable() const;
  bool IsFixed() const;
  unsigned long int Area() const;
  BlockOrient Orient() const;
  BlockAux *Aux() const;

  void SetType(BlockType *type);
  void SetLoc(double lx, double ly);
  void SetLLX(double lx);
  void SetLLY(double ly);
  void SetURX(double ux);
  void SetURY(double uy);
  void SetCenterX(double center_x);
  void SetCenterY(double center_y);
  void SetPlaceStatus(PlaceStatus place_status);
  void SetOrient(BlockOrient &orient);
  void SetAux(BlockAux *aux);
  void SwapLoc(Block &blk);

  void IncreX(double displacement);
  void IncreY(double displacement);
  void IncreX(double displacement, double upper, double lower);
  void IncreY(double displacement, double upper, double lower);
  void DecreX(double displacement);
  void DecreY(double displacement);
  bool IsOverlap(const Block &rhs) const;
  bool IsOverlap(const Block *rhs) const;
  double OverlapArea(const Block &rhs) const;
  double OverlapArea(const Block *rhs) const;

  void Report();
  void ReportNet();

  const std::string *TypeName() const;
  std::string GetPlaceStatusStr();
  std::string LowerLeftCorner();
};

class BlockAux {
 protected:
  Block *block_;
 public:
  explicit BlockAux(Block *block);
  Block *GetBlock();
};

inline const std::string *Block::Name() const {
  return &(name_num_pair_->first);
}

inline std::string Block::NameStr() const {
  return std::string(name_num_pair_->first);
}

inline BlockType *Block::Type() const {
  return type_;
}

inline int Block::Num() const {
  return name_num_pair_->second;
}

inline unsigned int Block::Width() const {
  return type_->Width();
}

inline unsigned int Block::Height() const {
  return type_->Height();
}

inline double Block::LLX() const {
  return llx_;
}

inline double Block::LLY() const {
  return lly_;
}

inline double Block::URX() const {
  return llx_ + Width();
}

inline double Block::URY() const {
  return lly_ + Height();
}

inline double Block::X() const {
  return llx_ + Width() / 2.0;
}

inline double Block::Y() const {
  return lly_ + Height() / 2.0;
}

inline bool Block::IsPlaced() const {
  return place_status_ == PLACED || place_status_ == FIXED;
}

inline PlaceStatus Block::GetPlaceStatus() {
  return place_status_;
}

inline bool Block::IsMovable() const {
  return place_status_ == UNPLACED || place_status_ == PLACED;
}

inline bool Block::IsFixed() const {
  return !IsMovable();
}

inline unsigned long int Block::Area() const {
  return type_->Area();
}

inline BlockOrient Block::Orient() const {
  return orient_;
}

inline BlockAux *Block::Aux() const {
  return aux_;
}

inline void Block::SetType(BlockType *type) {
  Assert(type != nullptr, "Cannot set BlockType of a Block to NULL");
  type_ = type;
}

inline void Block::SetLoc(double lx, double ly) {
  llx_ = lx;
  lly_ = ly;
}

inline void Block::SetLLX(double lx) {
  llx_ = lx;
}

inline void Block::SetLLY(double ly) {
  lly_ = ly;
}

inline void Block::SetURX(double ux) {
  llx_ = ux - Width();
}

inline void Block::SetURY(double uy) {
  lly_ = uy - Height();
}

inline void Block::SetCenterX(double center_x) {
  llx_ = center_x - Width() / 2.0;
}

inline void Block::SetCenterY(double center_y) {
  lly_ = center_y - Height() / 2.0;
}

inline void Block::SetPlaceStatus(PlaceStatus place_status) {
  place_status_ = place_status;
}

inline void Block::SetOrient(BlockOrient &orient) {
  orient_ = orient;
}

inline void Block::SetAux(BlockAux *aux) {
  Assert(aux != nullptr, "When set auxiliary information, argument cannot be a nullptr");
  aux_ = aux;
}

inline void Block::IncreX(double displacement) {
  llx_ += displacement;
}

inline void Block::IncreY(double displacement) {
  lly_ += displacement;
}

inline void Block::DecreX(double displacement) {
  llx_ -= displacement;
}

inline void Block::DecreY(double displacement) {
  lly_ -= displacement;
}

inline bool Block::IsOverlap(const Block &rhs) const {
  return !(LLX() > rhs.URX() || rhs.LLX() > URX() || LLY() > rhs.URY() || rhs.LLY() > URY());
}

inline bool Block::IsOverlap(const Block *rhs) const {
  return IsOverlap(*rhs);
}

inline double Block::OverlapArea(const Block *rhs) const {
  return OverlapArea(*rhs);
}

inline const std::string *Block::TypeName() const {
  return type_->Name();
}

inline std::string Block::GetPlaceStatusStr() {
  return PlaceStatusStr(place_status_);
}

inline std::string Block::LowerLeftCorner() {
  return "( " + std::to_string(LLX()) + " " + std::to_string(LLY()) + " )";
}

inline Block *BlockAux::GetBlock() {
  return block_;
}

#endif //DALI_BLOCK_HPP
