//
// Created by Yihang Yang on 3/14/20.
//

#ifndef DALI_SRC_PLACER_WELLLEGALIZER_STANDARDCLUSTERWELLLEGALIZER_H_
#define DALI_SRC_PLACER_WELLLEGALIZER_STANDARDCLUSTERWELLLEGALIZER_H_

#include "block_cluster.h"
#include "placer/placer.h"

struct Cluster {
  std::vector<Block *> blk_list_;
  int used_size_;
  int ly_;
  int height_;

  int UsedSize() const;
  void SetUsedSize(int used_size);

  void SetLLY(int ly);
  int LLY() const;
  int URY() const;
  double CenterY() const;

  void SetHeight(int height);
  int Height() const;

  void UpdateLocY();
};

struct ClusterColumn {
  std::vector<Cluster> clusters_;
  int lx_;
  int ux_;
  int width_;

  int top_;

  int clus_blk_cap_;

  int Width();

  void AppendBlock(Block &blk);
};

class StandardClusterWellLegalizer : public Placer {
 private:
  int max_unplug_length_;
  int plug_cell_width_;

  int max_cluster_width_;
  int col_width_;
  int tot_col_num_;

  std::vector<IndexLocPair<int>> index_loc_list_;
  std::vector<ClusterColumn> clus_cols_;

 public:
  StandardClusterWellLegalizer();

  void Init();

  int LocToCol(int x);
  void ClusterBlocks();

  void StartPlacement() override;
};

inline int Cluster::UsedSize() const {
  return used_size_;
}

inline void Cluster::SetUsedSize(int used_size) {
  used_size_ = used_size;
}

inline void Cluster::SetLLY(int ly) {
  ly_ = ly;
}

inline int Cluster::LLY() const {
  return ly_;
}

inline int Cluster::URY() const {
  return ly_ + height_;
}

inline double Cluster::CenterY() const {
  return ly_ + height_/2.0;
}

inline void Cluster::SetHeight(int height) {
  height_ = height;
}

inline int Cluster::Height() const {
  return height_;
}

inline int ClusterColumn::Width() {
  return width_;
}

inline int StandardClusterWellLegalizer::LocToCol(int x) {
  int col_num = (x - RegionLeft()) / col_width_;
  if (col_num < 0) {
    col_num = 0;
  }
  if (col_num >= tot_col_num_) {
    col_num = tot_col_num_ - 1;
  }
  return col_num;
}

#endif //DALI_SRC_PLACER_WELLLEGALIZER_STANDARDCLUSTERWELLLEGALIZER_H_
