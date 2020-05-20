//
// Created by Yihang Yang on 2020-01-30.
//

#include "block.h"

Block::Block() : ptype_(nullptr),
                 pname_num_pair_(nullptr),
                 llx_(0),
                 lly_(0),
                 place_status_(UNPLACED_),
                 orient_(N_),
                 paux_(nullptr) {
}

Block::Block(BlockType *ptype,
             std::pair<const std::string, int> *name_num_pair,
             int llx,
             int lly,
             bool movable,
             BlockOrient orient) :
    ptype_(ptype),
    pname_num_pair_(name_num_pair),
    llx_(llx),
    lly_(lly),
    orient_(orient) {
  eff_height_ = ptype_->Height();
  eff_area_ = ptype_->Area();
  Assert(name_num_pair != nullptr,
         "Must provide a valid pointer to the std::pair<std::string, int> element in the block_name_map");
  paux_ = nullptr;
  if (movable) {
    place_status_ = UNPLACED_;
  } else {
    place_status_ = FIXED_;
  }
}

Block::Block(BlockType *ptype,
             std::pair<const std::string, int> *name_num_pair,
             int llx,
             int lly,
             PlaceStatus place_state,
             BlockOrient orient) :
    ptype_(ptype),
    pname_num_pair_(name_num_pair),
    llx_(llx),
    lly_(lly),
    place_status_(place_state),
    orient_(orient) {
  eff_height_ = ptype_->Height();
  eff_area_ = ptype_->Area();
  Assert(name_num_pair != nullptr,
         "Must provide a valid pointer to the std::pair<std::string, int> element in the block_name_map");
  paux_ = nullptr;
}

void Block::SwapLoc(Block &blk) {
  double tmp_x = llx_;
  double tmp_y = lly_;
  llx_ = blk.LLX();
  lly_ = blk.LLY();
  blk.SetLLX(tmp_x);
  blk.SetLLY(tmp_y);
}

void Block::IncreaseX(double displacement, double upper, double lower) {
  llx_ += displacement;
  double real_upper = upper - Width();
  if (llx_ < lower) {
    llx_ = lower;
  } else if (llx_ > real_upper) {
    llx_ = real_upper;
  }
}

void Block::IncreaseY(double displacement, double upper, double lower) {
  lly_ += displacement;
  double real_upper = upper - Height();
  if (lly_ < lower) {
    lly_ = lower;
  } else if (lly_ > real_upper) {
    lly_ = real_upper;
  }
}

double Block::OverlapArea(const Block &rhs) const {
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

void Block::Report() {
  std::cout << "  block name: " << *Name() << "\n"
            << "    block type: " << *(Type()->Name()) << "\n"
            << "    width and height: " << Width() << " " << Height() << "\n"
            << "    lower left corner: " << llx_ << " " << lly_ << "\n"
            << "    movable: " << IsMovable() << "\n"
            << "    orientation: " << OrientStr(orient_) << "\n"
            << "    assigned primary key: " << Num()
            << "\n";
}

void Block::ReportNet() {
  std::cout << *Name() << " connects to:\n";
  for (auto &net_num: net_list) {
    std::cout << net_num << "  ";
  }
  std::cout << "\n";
}
