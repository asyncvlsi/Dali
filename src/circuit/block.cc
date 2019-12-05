//
// Created by Yihang Yang on 2019-05-23.
//

#include "block.h"

Block::Block(BlockType *type, std::pair<const std::string, int>* name_num_pair, int llx, int lly, bool movable, BlockOrient orient) : type_(
    type), name_num_pair_(name_num_pair), llx_(llx), lly_(lly), orient_(orient) {
  Assert(name_num_pair != nullptr, "Must provide a valid pointer to the std::pair<std::string, int> element in the block_name_map");
  aux_ = nullptr;
  if (movable) {
    place_status_ = UNPLACED;
  } else {
    place_status_ = FIXED;
  }
}

Block::Block(BlockType *type, std::pair<const std::string, int>* name_num_pair, int llx, int lly, PlaceStatus place_state, BlockOrient orient) :
    type_(type), name_num_pair_(name_num_pair), llx_(llx), lly_(lly), place_status_(place_state), orient_(orient) {
  Assert(name_num_pair != nullptr, "Must provide a valid pointer to the std::pair<std::string, int> element in the block_name_map");
  aux_ = nullptr;
}

void Block::SetLLX(double lower_left_x) {
  llx_ = lower_left_x;
}

double Block::LLX() const{
  return llx_;
}

void Block::SetLLY(double lower_left_y) {
  lly_ = lower_left_y;
}

double Block::LLY() const{
  return lly_;
}

void Block::SetURX(double upper_right_x) {
  llx_ = upper_right_x - Width();
}

double Block::URX() const{
  return llx_ + Width();
}

void Block::SetURY(double upper_right_y) {
  lly_ = upper_right_y - Height();
}

double Block::URY() const{
  return lly_ + Height();
}

void Block::SetCenterX(double center_x) {
  llx_ = center_x - Width()/2.0;
}

double Block::X() const{
  return llx_ + Width()/2.0;
}

void Block::SetCenterY(double center_y) {
  lly_ = center_y - Height()/2.0;
}

double Block::Y() const{
  return lly_ + Height()/2.0;
}

bool Block::IsMovable() const {
  return place_status_==UNPLACED || place_status_==PLACED;
}

bool Block::IsFixed() const {
  return !IsMovable();
}

void Block::SetOrient(BlockOrient &orient) {
  orient_ = orient;
}

void Block::IncreX(double displacement) {
  llx_ += displacement;
}

void Block::IncreY(double displacement) {
  lly_ += displacement;
}

void Block::IncreX(double displacement, double upper, double lower) {
  llx_ += displacement;
  double real_upper = upper - Width();
  if (llx_ < lower) llx_ = lower;
  else if (llx_ > real_upper) llx_ = real_upper;
}

void Block::IncreY(double displacement, double upper, double lower) {
  lly_ += displacement;
  double real_upper = upper - Height();
  if (lly_ < lower) lly_ = lower;
  else if (lly_ > real_upper) lly_ = real_upper;
}

bool Block::IsOverlap(const Block &rhs) const {
  bool not_overlap = LLX() > rhs.URX() || rhs.LLX() > URX() || LLY() > rhs.URY() || rhs.LLY() > URY();
  return !not_overlap;
}

void Block::SetAux(BlockAux *aux) {
  Assert(aux != nullptr, "When set auxiliary information, argument cannot be a nullptr");
  aux_ = aux;
}

double Block::OverlapArea(const Block &rhs) const {
  double overlap_area = 0;
  if (IsOverlap(rhs)) {
    double llx, urx, lly, ury;
    llx = std::max(LLX(), rhs.LLX());
    urx = std::min(URX(), rhs.URX());
    lly = std::max(LLY(), rhs.LLY());
    ury = std::min(URY(), rhs.URY());
    overlap_area = (urx - llx)*(ury - lly);
  }
  return overlap_area;
}

double Block::OverlapArea(const Block *rhs) const {
  return OverlapArea(*rhs);
}

void Block::Report() {
  std::cout << "  block name: " << *Name() << "\n"
            << "    block type: " << *(Type()->Name()) << "\n"
            << "    width and height: " << Width() << " " << Height() << "\n"
            << "    lower left corner: " << LLX() << " " << LLY() << "\n"
            << "    movable: " << IsMovable() << "\n"
            << "    orientation: " << OrientStr(orient_) << "\n"
            << "    assigned primary key: " << Num()
            << "\n";
}

void Block::ReportNet() {
  std::cout << *Name() << " connects to:\n";
  for (auto &&net_num: net_list) {
    std::cout << net_num << "  ";
  }
  std::cout << "\n";
}

void Block::SwapLoc(Block &blk) {
  double tmp_x = llx_;
  double tmp_y = lly_;
  llx_ = blk.LLX();
  lly_ = blk.LLY();
  blk.SetLLX(tmp_x);
  blk.SetLLY(tmp_y);
}
