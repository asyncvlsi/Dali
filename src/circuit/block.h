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
  bool is_placed = false;
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
  bool IsFixed() const;
  int Area() const;
  void SetOrient(BlockOrient &orient);
  BlockOrient Orient() const;
  std::string OrientStr() const;
  void IncreX(double displacement);
  void IncreY(double displacement);
  void IncreX(double displacement, double upper, double lower);
  void IncreY(double displacement, double upper, double lower);
  bool IsOverlap(const Block &rhs) const;
  bool IsOverlap(const Block *rhs) const;
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

inline const std::string *Block::Name() const{
  return &(name_num_pair_ptr_->first);
}

inline BlockType *Block::Type() const {
  return type_;
}

inline int Block::Num() const {
  return name_num_pair_ptr_->second;
}

inline int Block::Width() const{
  return type_->Width();
}

inline int Block::Height() const{
  return type_->Height();
}

inline void Block::SetLLX(double lower_left_x) {
  llx_ = lower_left_x;
}

inline double Block::LLX() const{
  return llx_;
}

inline void Block::SetLLY(double lower_left_y) {
  lly_ = lower_left_y;
}

inline double Block::LLY() const{
  return lly_;
}

inline void Block::SetURX(double upper_right_x) {
  llx_ = upper_right_x - Width();
}

inline double Block::URX() const{
  return llx_ + Width();
}

inline void Block::SetURY(double upper_right_y) {
  lly_ = upper_right_y - Height();
}

inline double Block::URY() const{
  return lly_ + Height();
}

inline void Block::SetCenterX(double center_x) {
  llx_ = center_x - Width()/2.0;
}

inline double Block::X() const{
  return llx_ + Width()/2.0;
}

inline void Block::SetCenterY(double center_y) {
  lly_ = center_y - Height()/2.0;
}

inline double Block::Y() const{
  return lly_ + Height()/2.0;
}

inline void Block::SetMovable(bool movable) {
  movable_ = movable;
}

inline bool Block::IsMovable() const {
  return movable_;
}

inline bool Block::IsFixed() const {
  return !movable_;
}

inline int Block::Area() const {
  return Height() * Width();
}

inline void Block::SetOrient(BlockOrient &orient) {
  orient_ = orient;
}

inline BlockOrient Block::Orient() const {
  return orient_;
}

inline void Block::IncreX(double displacement) {
  llx_ += displacement;
}

inline void Block::IncreY(double displacement) {
  lly_ += displacement;
}

inline void Block::IncreX(double displacement, double upper, double lower) {
  llx_ += displacement;
  double real_upper = upper - Width();
  if (llx_ < lower) llx_ = lower;
  else if (llx_ > real_upper) llx_ = real_upper;
}

inline void Block::IncreY(double displacement, double upper, double lower) {
  lly_ += displacement;
  double real_upper = upper - Height();
  if (lly_ < lower) lly_ = lower;
  else if (lly_ > real_upper) lly_ = real_upper;
}

inline bool Block::IsOverlap(const Block &rhs) const {
  bool not_overlap = LLX() > rhs.URX() || rhs.LLX() > URX() || LLY() > rhs.URY() || rhs.LLY() > URY();
  return !not_overlap;
}

inline bool Block::IsOverlap(const Block *rhs) const {
  return IsOverlap(*rhs);
}

inline void Block::SetAux(BlockAux *aux) {
  Assert(aux != nullptr, "When set auxiliary information, argument cannot be a nullptr");
  aux_ = aux;
}

inline BlockAux *Block::Aux(){
  return aux_;
}

inline const std::string *Block::TypeName() const {
  return type_->Name();
}

inline std::string Block::IsPlace() {
  return "PLACED";
}

inline std::string Block::LowerLeftCorner() {
  return "( " + std::to_string(LLX()) + " " + std::to_string(LLY()) + " )";
}

#endif //HPCC_BLOCK_HPP
