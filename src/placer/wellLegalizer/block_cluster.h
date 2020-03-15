//
// Created by Yihang Yang on 3/14/20.
//

#ifndef DALI_SRC_PLACER_WELLLEGALIZER_BLOCK_CLUSTER_H_
#define DALI_SRC_PLACER_WELLLEGALIZER_BLOCK_CLUSTER_H_

#include "circuit/block.h"

struct BlkCluster {
  BlkCluster();
  BlkCluster(int well_extension_x_init, int well_extension_y_init, int plug_width_init);

  int well_extension_x_;
  int well_extension_y_;
  int plug_width_;

  int p_well_height_;

  int width_;
  int height_;
  int lx_;
  int ly_;
  std::vector<Block *> blk_ptr_list_;

  // cached value;
  int modified_lx_;

  int Width() const;
  int Height() const;
  long int Area() const;
  int InnerUX() const;
  int LLX() const;
  int LLY() const;
  int URX() const;
  int URY() const;
  double CenterX() const;
  double CenterY() const;

  int size() const;

  void SetLLX(int lx);
  void SetLLY(int ly);
  void SetURX(int ux);
  void SetURY(int uy);
  void SetLoc(int lx, int ly);
  void SetCenterX(double center_x);
  void SetCenterY(double center_y);
  void IncreX(int displacement);
  void IncreY(int displacement);

  void AppendBlock(Block &block);
  void OptimizeHeight();
  void UpdateBlockLocation();
};

inline int BlkCluster::Width() const {
  return width_;
}

inline int BlkCluster::Height() const {
  return height_;
}

inline long int BlkCluster::Area() const {
  return (long int) width_ * (long int) height_;
}

inline int BlkCluster::InnerUX() const {
  return modified_lx_ + width_;
}

inline int BlkCluster::LLX() const {
  return lx_;
}

inline int BlkCluster::LLY() const {
  return ly_;
}

inline int BlkCluster::URX() const {
  return lx_ + width_;
}

inline int BlkCluster::URY() const {
  return ly_ + height_;
}

inline double BlkCluster::CenterX() const {
  return lx_ + width_ / 2.0;
}

inline double BlkCluster::CenterY() const {
  return ly_ + height_ / 2.0;
}

inline int BlkCluster::size() const {
  return blk_ptr_list_.size();
}

inline void BlkCluster::SetLLX(int lx) {
  lx_ = lx;
}

inline void BlkCluster::SetLLY(int ly) {
  ly_ = ly;
}

inline void BlkCluster::SetURX(int ux) {
  lx_ = ux - width_;
}

inline void BlkCluster::SetURY(int uy) {
  ly_ = uy - height_;
}

inline void BlkCluster::SetLoc(int lx, int ly) {
  lx_ = lx;
  ly_ = ly;
}

inline void BlkCluster::SetCenterX(double center_x) {
  lx_ = (int) std::round(center_x - width_ / 2.0);
}

inline void BlkCluster::SetCenterY(double center_y) {
  ly_ = (int) std::round(center_y - height_ / 2.0);
}

inline void BlkCluster::IncreX(int displacement) {
  lx_ += displacement;
}

inline void BlkCluster::IncreY(int displacement) {
  ly_ += displacement;
}

struct CluPtrLocPair {
  BlkCluster *clus_ptr;
  int x;
  int y;
  explicit CluPtrLocPair(BlkCluster *clus_ptr_init = nullptr, int x_init = 0, int y_init = 0)
      : clus_ptr(clus_ptr_init), x(x_init), y(y_init) {}
  bool operator<(const CluPtrLocPair &rhs) const {
    return (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
  }
};

#endif //DALI_SRC_PLACER_WELLLEGALIZER_BLOCK_CLUSTER_H_
