//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_SRC_CIRCUIT_PIN_H_
#define DALI_SRC_CIRCUIT_PIN_H_

#include <vector>

#include "blocktype.h"
#include "common/misc.h"
#include "status.h"

class BlockType;

class Pin {
 private:
  /*** essential data entries ****/
  std::vector<RectD> rect_list_;
  std::pair<const std::string, int> *pname_num_pair_;
  BlockType *blk_type_;

  bool is_input_;
  bool manual_set_;
  std::vector<double> x_offset_;
  std::vector<double> y_offset_;

 public:
  Pin(std::pair<const std::string, int> *name_num_pair_ptr, BlockType *blk_type);
  Pin(std::pair<const std::string, int> *name_num_pair_ptr, BlockType *blk_type, double x_offset, double y_offset);

  const std::string *Name() const { return &(pname_num_pair_->first); }
  int Num() const { return pname_num_pair_->second; }

  void InitOffset();
  void CalculateOffset(double x_offset, double y_offset);
  void SetOffset(double x_offset, double y_offset) {
    CalculateOffset(x_offset, y_offset);
    manual_set_ = true;
  }
  double OffsetX(BlockOrient orient = N_) const { return x_offset_[orient - N_]; }
  double OffsetY(BlockOrient orient = N_) const { return y_offset_[orient - N_]; }

  void AddRect(RectD &rect) {
    if (rect_list_.empty()) {
      CalculateOffset((rect.LLX() + rect.URX()) / 2.0, (rect.LLY() + rect.URY()) / 2.0);
    }
    rect_list_.push_back(rect);
  }
  void AddRect(double llx, double lly, double urx, double ury) {
    if (rect_list_.empty()) {
      CalculateOffset((llx + urx) / 2.0, (lly + ury) / 2.0);
    }
    rect_list_.emplace_back(llx, lly, urx, ury);
  }

  bool GetIOType() const { return is_input_; }
  void SetIOType(bool is_input) { is_input_ = is_input; }

  bool RectEmpty() const { return rect_list_.empty(); }
  void Report() const {
    std::cout << *Name() << " (" << OffsetX() << ", " << OffsetY() << ")";
  }
};

#endif //DALI_SRC_CIRCUIT_PIN_H_
