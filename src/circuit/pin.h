//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_SRC_CIRCUIT_PIN_H_
#define DALI_SRC_CIRCUIT_PIN_H_

#include <vector>

#include "common/misc.h"
#include "status.h"
#include "blocktype.h"

class BlockType;

class Pin {
 private:
  /*** essential data entries ****/
  std::vector<RectD> rect_list_;
  std::pair<const std::string, int> *name_num_pair_ptr_;
  BlockType *blk_type_;

  bool is_input_;
  bool manual_set_;
  std::vector<double> x_offset_;
  std::vector<double> y_offset_;

 public:
  Pin(std::pair<const std::string, int> *name_num_pair_ptr, BlockType *blk_type);
  Pin(std::pair<const std::string, int> *name_num_pair_ptr, BlockType *blk_type, double x_offset, double y_offset);

  const std::string *Name() const;
  int Num() const;

  void InitOffset();
  void CalculateOffset(double x_offset, double y_offset);
  void SetOffset(double x_offset, double y_offset);
  double OffsetX(BlockOrient orient = N) const;
  double OffsetY(BlockOrient orient = N) const;

  void AddRect(RectD &rect);
  void AddRect(double llx, double lly, double urx, double ury);

  bool GetIOType() const;
  void SetIOType(bool is_input);

  bool Empty() const;
  void Report() const;
};

inline const std::string *Pin::Name() const {
  return &(name_num_pair_ptr_->first);
}

inline int Pin::Num() const {
  return name_num_pair_ptr_->second;
}

inline void Pin::SetOffset(double x_offset, double y_offset) {
  CalculateOffset(x_offset, y_offset);
  manual_set_ = true;
}

inline double Pin::OffsetX(BlockOrient orient) const {
  return x_offset_[orient - N];
}

inline double Pin::OffsetY(BlockOrient orient) const {
  return y_offset_[orient - N];
}

inline void Pin::AddRect(RectD &rect) {
  rect_list_.push_back(rect);
}

inline void Pin::AddRect(double llx, double lly, double urx, double ury) {
  if (rect_list_.empty()) {
    CalculateOffset((llx + urx) / 2.0, (lly + ury) / 2.0);
  }
  rect_list_.emplace_back(llx, lly, urx, ury);
}

inline bool Pin::GetIOType() const {
  return is_input_;
}

inline void Pin::SetIOType(bool is_input) {
  is_input_ = is_input;
}

inline bool Pin::Empty() const {
  return rect_list_.empty();
}

inline void Pin::Report() const {
  std::cout << *Name() << " (" << OffsetX() << ", " << OffsetY() << ")";
}

#endif //DALI_SRC_CIRCUIT_PIN_H_
