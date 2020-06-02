//
// Created by Yihang Yang on 2019-05-23.
//

#include "block.h"

Block::Block() : type_ptr_(nullptr),
                 name_num_pair_ptr_(nullptr),
                 llx_(0),
                 lly_(0),
                 place_status_(UNPLACED_),
                 orient_(N_),
                 aux_ptr_(nullptr) {
}

Block::Block(BlockType *type_ptr,
             std::pair<const std::string, int> *name_num_pair_ptr,
             int llx,
             int lly,
             bool movable,
             BlockOrient orient) :
    type_ptr_(type_ptr),
    name_num_pair_ptr_(name_num_pair_ptr),
    llx_(llx),
    lly_(lly),
    orient_(orient) {
  eff_height_ = type_ptr_->Height();
  eff_area_ = type_ptr_->Area();
  Assert(name_num_pair_ptr != nullptr,
         "Must provide a valid pointer to the std::pair<std::string, int> element in the block_name_map");
  aux_ptr_ = nullptr;
  if (movable) {
    place_status_ = UNPLACED_;
  } else {
    place_status_ = FIXED_;
  }
}

Block::Block(BlockType *type_ptr,
             std::pair<const std::string, int> *name_num_pair_ptr,
             int llx,
             int lly,
             PlaceStatus place_state,
             BlockOrient orient) :
    type_ptr_(type_ptr),
    name_num_pair_ptr_(name_num_pair_ptr),
    llx_(llx),
    lly_(lly),
    place_status_(place_state),
    orient_(orient) {
  eff_height_ = type_ptr_->Height();
  eff_area_ = type_ptr_->Area();
  Assert(name_num_pair_ptr != nullptr,
         "Must provide a valid pointer to the std::pair<std::string, int> element in the block_name_map");
  aux_ptr_ = nullptr;
}

void Block::SwapLoc(Block &blk) {
  double tmp_x = llx_;
  double tmp_y = lly_;
  llx_ = blk.LLX();
  lly_ = blk.LLY();
  blk.setLLX(tmp_x);
  blk.setLLY(tmp_y);
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

double Block::OverlapArea(const Block &blk) const {
  double overlap_area = 0;
  if (IsOverlap(blk)) {
    double llx, urx, lly, ury;
    llx = std::max(LLX(), blk.LLX());
    urx = std::min(URX(), blk.URX());
    lly = std::max(LLY(), blk.LLY());
    ury = std::min(URY(), blk.URY());
    overlap_area = (urx - llx) * (ury - lly);
  }
  return overlap_area;
}

void Block::Report() {
  std::cout << "  block name: " << *NamePtr() << "\n"
            << "    block type: " << *(Type()->NamePtr()) << "\n"
            << "    width and height: " << Width() << " " << Height() << "\n"
            << "    lower left corner: " << llx_ << " " << lly_ << "\n"
            << "    movable: " << IsMovable() << "\n"
            << "    orientation: " << OrientStr(orient_) << "\n"
            << "    assigned primary key: " << Num()
            << "\n";
}

void Block::ReportNet() {
  std::cout << *NamePtr() << " connects to:\n";
  for (auto &net_num: net_list_) {
    std::cout << net_num << "  ";
  }
  std::cout << "\n";
}
