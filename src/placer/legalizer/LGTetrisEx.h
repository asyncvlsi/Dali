//
// Created by Yihang Yang on 1/2/20.
//

#ifndef DALI_SRC_PLACER_LEGALIZER_LGTETRISEX_H_
#define DALI_SRC_PLACER_LEGALIZER_LGTETRISEX_H_

#include "common/misc.h"
#include "placer/placer.h"

class LGTetrisEx : public Placer {
 protected:
  std::vector<std::vector<SegI>> white_space_in_rows_;
  std::vector<int> block_contour_;
  std::vector<IndexLocPair<int>> index_loc_list_;

  int row_height_;

  bool legalize_from_left_;

  int cur_iter_;
  int max_iter_;

  double k_width_;
  double k_height_;
  double k_left_;

  //cached data
  int tot_num_rows_;

 public:
  LGTetrisEx();

  void SetRowHeight(int row_height);
  int RowHeight();
  void SetMaxIteration(int max_iter);
  void SetWidthHeightFactor(double k_width, double k_height);
  void SetLeftBoundFactor(double k_left);

  static void MergeIntervals(std::vector<std::vector<int>> &intervals);
  void InitLegalizer();

  int StartRow(int y_loc);
  int EndRow(int y_loc);
  int MaxRow(int height);
  int HeightToRow(int height);
  int LocToRow(int y_loc);
  int RowToLoc(int row_num, int displacement = 0);
  int AlignedLocToRow(int y_loc);
  int AlignedLocToRow(double y_loc);
  virtual bool IsSpaceLegal(int lo_x, int hi_x, int lo_row, int hi_row);

  virtual void UseSpaceLeft(Block const &block);
  virtual bool IsCurrentLocLegalLeft(Value2D<int> &loc, int width, int height);
  virtual int WhiteSpaceBoundLeft(int lo_x, int hi_x, int lo_row, int hi_row);
  virtual bool FindLocLeft(Value2D<int> &loc, int width, int height);
  void FastShiftLeft(int failure_point);
  bool LocalLegalizationLeft();

  void UseSpaceRight(Block const &block);
  bool IsCurrentLocLegalRight(Value2D<int> &loc, int width, int height);
  int WhiteSpaceBoundRight(int lo_x, int hi_x, int lo_row, int hi_row);
  bool FindLocRight(Value2D<int> &loc, int width, int height);
  void FastShiftRight(int failure_point);
  bool LocalLegalizationRight();

  double EstimatedHPWL(Block &block, int x, int y);

  void StartPlacement() override;

  void GenAvailSpace(std::string const &name_of_file = "avail_space.txt");
};

inline void LGTetrisEx::SetRowHeight(int row_height) {
  Assert(row_height > 0, "Cannot set negative row height!");
  row_height_ = row_height;
}

inline int LGTetrisEx::RowHeight() {
  return row_height_;
}

inline void LGTetrisEx::SetMaxIteration(int max_iter) {
  assert(max_iter >= 0);
  max_iter_ = max_iter;
}

inline void LGTetrisEx::SetWidthHeightFactor(double k_width, double k_height) {
  k_width_ = k_width;
  k_height_ = k_height;
}

inline void LGTetrisEx::SetLeftBoundFactor(double k_left) {
  k_left_ = k_left;
}

inline int LGTetrisEx::StartRow(int y_loc) {
  return (y_loc - bottom_) / row_height_;
}

inline int LGTetrisEx::EndRow(int y_loc) {
  int relative_y = y_loc - bottom_;
  int res = relative_y / row_height_;
  if (relative_y % row_height_ == 0) {
    --res;
  }
  return res;
}

inline int LGTetrisEx::MaxRow(int height) {
  return ((top_ - height) - bottom_) / row_height_;
}

inline int LGTetrisEx::HeightToRow(int height) {
  return std::ceil(height / double(row_height_));
}

inline int LGTetrisEx::LocToRow(int y_loc) {
  return (y_loc - bottom_) / row_height_;
}

inline int LGTetrisEx::RowToLoc(int row_num, int displacement) {
  return row_num * row_height_ + bottom_ + displacement;
}

inline int LGTetrisEx::AlignedLocToRow(int y_loc) {
  int row_num = int(std::round((y_loc - bottom_) / (double) row_height_));
  if (row_num < 0) row_num = 0;
  if (row_num >= tot_num_rows_) row_num = tot_num_rows_ - 1;
  return row_num * row_height_ + bottom_;
}

inline int LGTetrisEx::AlignedLocToRow(double y_loc) {
  int row_num = int(std::round((y_loc - bottom_) / row_height_));
  if (row_num < 0) row_num = 0;
  if (row_num >= tot_num_rows_) row_num = tot_num_rows_ - 1;
  return row_num * row_height_ + bottom_;
}

#endif //DALI_SRC_PLACER_LEGALIZER_LGTETRISEX_H_
