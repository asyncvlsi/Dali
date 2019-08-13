//
// Created by Yihang Yang on 2019-05-23.
//

#include "block.h"

Block::Block(BlockType *type, std::pair<const std::string, int>* name_num_pair_ptr, int llx, int lly, bool movable, BlockOrient orient) : type_(
    type), name_num_pair_ptr_(name_num_pair_ptr), llx_(llx), lly_(lly), movable_(movable), orient_(orient) {
  Assert(name_num_pair_ptr != nullptr, "Must provide a valid pointer to the std::pair<std::string, int> element in the block_name_map");
  aux_ = nullptr;
}

const std::string *Block::Name() const{
  return &(name_num_pair_ptr_->first);
}

BlockType *Block::Type() const {
  return type_;
}

int Block::Num() const {
  return name_num_pair_ptr_->second;
}

int Block::Width() const{
  return type_->Width();
}

int Block::Height() const{
  return type_->Height();
}

void Block::SetLLX(int lower_left_x) {
  llx_ = lower_left_x;
}

int Block::LLX() const{
  return llx_;
}

void Block::SetLLY(int lower_left_y) {
  lly_ = lower_left_y;
}

int Block::LLY() const{
  return lly_;
}

void Block::SetURX(int upper_right_x) {
  llx_ = upper_right_x - Width();
}

int Block::URX() const{
  return llx_ + Width();
}

void Block::SetURY(int upper_right_y) {
  lly_ = upper_right_y - Height();
}

int Block::URY() const{
  return lly_ + Height();
}

void Block::SetCenterX(double center_x) {
  llx_ = (int) (center_x - Width()/2.0);
}

double Block::X() const{
  return llx_ + Width()/2.0;
}

void Block::SetCenterY(double center_y) {
  lly_ = (int) (center_y - Height()/2.0);
}

double Block::Y() const{
  return lly_ + Height()/2.0;
}

void Block::SetMovable(bool movable) {
  movable_ = movable;
}

bool Block::IsMovable() const {
  return movable_;
}

int Block::Area() const {
  return Height() * Width();
}

void Block::SetOrient(BlockOrient &orient) {
  orient_ = orient;
}

BlockOrient Block::Orient() const {
  return orient_;
}

std::string Block::OrientStr() const {
  std::string s;
  switch (orient_) {
    case 0: { s = "N"; } break;
    case 1: { s = "S"; } break;
    case 2: { s = "W"; } break;
    case 3: { s = "E"; } break;
    case 4: { s = "FN"; } break;
    case 5: { s = "FS"; } break;
    case 6: { s = "FW"; } break;
    case 7: { s = "FE"; } break;
    default: {
      Assert(false, "Block orientation error! This should not happen!");
    }
  }
  return s;
}

void Block::SetAux(BlockAux *aux) {
  Assert(aux != nullptr, "When set auxiliary information, argument cannot be a nullptr");
  aux_ = aux;
}

BlockAux *Block::Aux(){
  return aux_;
}

void Block::IncreX(double amount) {
  llx_ += amount;
}

void Block::IncreY(double amount) {
  lly_ += amount;
}

const std::string *Block::TypeName() const {
  return type_->Name();
}

std::string Block::IsPlace() {
  return "PLACED";
}

std::string Block::LowerLeftCorner() {
  return "( " + std::to_string(LLX()) + " " + std::to_string(LLY()) + " )";
}



