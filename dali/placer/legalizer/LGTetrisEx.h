//
// Created by Yihang Yang on 1/2/20.
//

#ifndef DALI_SRC_PLACER_LEGALIZER_LGTETRISEX_H_
#define DALI_SRC_PLACER_LEGALIZER_LGTETRISEX_H_

#include "dali/common/displaceviewer.h"
#include "dali/common/misc.h"
#include "dali/placer/placer.h"

namespace dali {

class LGTetrisEx : public Placer {
 protected:
  std::vector<std::vector<SegI>> white_space_in_rows_;
  std::vector<int> block_contour_;
  std::vector<int> block_contour_y_;
  std::vector<IndexLocPair<int>> index_loc_list_;

  int row_height_;
  bool row_height_set_;

  bool legalize_from_left_;

  int cur_iter_;
  int max_iter_;

  double k_width_;
  double k_height_;
  double k_left_;

  //cached data
  int tot_num_rows_;
  int tot_num_cols_;

  // displacement visualization
  bool view_displacement_ = false;
  DisplaceViewer<double> *displace_viewer_ = nullptr;

 public:
  LGTetrisEx();

  void SetRowHeight(int row_height) {
    DaliExpects(row_height > 0, "Cannot set negative row height!");
    row_height_ = row_height;
    row_height_set_ = true;
  }
  int RowHeight() const { return row_height_; }
  void SetMaxIteration(int max_iter) {
    DaliExpects(max_iter >= 0, "Cannot set negative maximum iteration @prarm max_iter: LGTetrisEx::SetMaxIteration()\n");
    max_iter_ = max_iter;
  }
  void SetWidthHeightFactor(double k_width, double k_height) {
    k_width_ = k_width;
    k_height_ = k_height;
  }
  void SetLeftBoundFactor(double k_left) { k_left_ = k_left; }

  static void MergeIntervals(std::vector<std::vector<int>> &intervals);
  void InitLegalizer();
  void InitLegalizerY();

  int StartRow(int y_loc) { return (y_loc - bottom_) / row_height_; }
  int StartCol(int x_loc) { return x_loc - left_; }
  int EndRow(int y_loc) {
    int relative_y = y_loc - bottom_;
    int res = relative_y / row_height_;
    if (relative_y % row_height_ == 0) {
      --res;
    }
    return res;
  }
  int EndCol(int x_loc) { return x_loc - left_ - 1; }
  int MaxRow(int height) { return ((top_ - height) - bottom_) / row_height_; }
  int MaxCol(int width) { return right_ - width - left_; }
  int HeightToRow(int height) const { return std::ceil(height / double(row_height_)); }
  int LocToRow(int y_loc) { return (y_loc - bottom_) / row_height_; }
  int LocToCol(int x_loc) { return x_loc - left_; }
  int RowToLoc(int row_num, int displacement = 0) { return row_num * row_height_ + bottom_ + displacement; }
  int ColToLoc(int col_num, int displacement = 0) { return col_num + left_ + displacement; }
  int AlignLocToRow(int y_loc) {
    int row_num = int(std::round((y_loc - bottom_) / (double) row_height_));
    if (row_num < 0) row_num = 0;
    if (row_num >= tot_num_rows_) row_num = tot_num_rows_ - 1;
    return row_num * row_height_ + bottom_;
  }
  int AlignLocToRowLoc(double y_loc) {
    int row_num = (int) std::round((y_loc - bottom_) / row_height_);
    if (row_num < 0) row_num = 0;
    if (row_num >= tot_num_rows_) row_num = tot_num_rows_ - 1;
    return row_num * row_height_ + bottom_;
  }
  int AlignLocToColLoc(double x_loc) {
    int col_num = (int) std::round(x_loc - left_);
    if (col_num < 0) col_num = 0;
    if (col_num >= tot_num_cols_) col_num = tot_num_cols_ - 1;
    return col_num + left_;
  }
  bool IsSpaceLegal(int lo_x, int hi_x, int lo_row, int hi_row);

  void UseSpaceLeft(Block const &block);
  bool IsCurrentLocLegalLeft(Value2D<int> &loc, int width, int height);
  int WhiteSpaceBoundLeft(int lo_x, int hi_x, int lo_row, int hi_row);
  bool FindLocLeft(Value2D<int> &loc, int width, int height);
  void FastShiftLeft(int failure_point);
  bool LocalLegalizationLeft();

  void UseSpaceRight(Block const &block);
  bool IsCurrentLocLegalRight(Value2D<int> &loc, int width, int height);
  int WhiteSpaceBoundRight(int lo_x, int hi_x, int lo_row, int hi_row);
  bool FindLocRight(Value2D<int> &loc, int width, int height);
  void FastShiftRight(int failure_point);
  bool LocalLegalizationRight();

  void UseSpaceBottom(Block const &block);
  bool IsCurrentLocLegalBottom(Value2D<int> &loc, int width, int height);
  virtual bool FindLocBottom(Value2D<int> &loc, int width, int height);
  bool LocalLegalizationBottom();

  void UseSpaceTop(Block const &block);
  bool IsCurrentLocLegalTop(Value2D<int> &loc, int width, int height);
  virtual bool FindLocTop(Value2D<int> &loc, int width, int height);
  bool LocalLegalizationTop();

  double EstimatedHPWL(Block &block, int x, int y);

  bool StartPlacement() override;

  void GenAvailSpace(std::string const &name_of_file = "avail_space.txt");

  void InitDispViewer();
  void IsPrintDisplacement(bool view_displacement = false) {
    view_displacement_ = view_displacement;
    InitDispViewer();
  }
  void GenDisplacement(std::string const &name_of_file = "disp_result.txt");
};

}

#endif //DALI_SRC_PLACER_LEGALIZER_LGTETRISEX_H_
