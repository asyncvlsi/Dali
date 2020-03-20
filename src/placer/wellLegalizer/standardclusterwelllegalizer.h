//
// Created by Yihang Yang on 3/14/20.
//

#ifndef DALI_SRC_PLACER_WELLLEGALIZER_STANDARDCLUSTERWELLLEGALIZER_H_
#define DALI_SRC_PLACER_WELLLEGALIZER_STANDARDCLUSTERWELLLEGALIZER_H_

#include "block_cluster.h"
#include "common/misc.h"
#include "placer/placer.h"

struct Cluster {
  std::vector<Block *> blk_list_;
  int used_size_;
  int lx_;
  int width_;
  int ly_;
  int height_;

  int UsedSize() const;
  void SetUsedSize(int used_size);
  void UseSpace(int width);

  void SetLLX(int lx);
  void SetURX(int ux);
  int LLX() const;
  int URX() const;
  double CenterX() const;

  void SetWidth(int width);
  int Width() const;

  void SetLLY(int ly);
  void SetURY(int uy);
  int LLY() const;
  int URY() const;
  double CenterY() const;

  void SetHeight(int height);
  int Height() const;

  void SetLoc(int lx, int ly);

  void ShiftBlockX(int x_disp);
  void ShiftBlockY(int y_disp);
  void ShiftBlock(int x_disp, int y_disp);
  void UpdateLocY();
  void LegalizeCompactX(int left);
  void LegalizeLooseX(int left, int right);

  void UpdateBlockLocationCompact();
};

struct ClusterColumn {
  int lx_;
  int width_;
  int max_blk_capacity_per_cluster_;

  int contour_;
  int used_height_;
  int cluster_count_;
  Cluster *top_cluster_;

  int Width() const;
  int LLX() const;
  int URX() const;
};

class StandardClusterWellLegalizer : public Placer {
 private:
  int max_unplug_length_;
  int plug_cell_width_;

  int max_cluster_width_;
  int col_width_;
  int tot_col_num_;

  std::vector<IndexLocPair<int>> index_loc_list_;
  std::vector<Cluster> cluster_list_;
  std::vector<ClusterColumn> clus_cols_;

 public:
  StandardClusterWellLegalizer();

  void Init();

  int LocToCol(int x);
  void AppendBlockToCol(int col_num, Block &blk);
  void AppendBlockToColClose(int col_num, Block &blk);
  void ClusterBlocks();
  void ClusterBlocksLoose();
  void ClusterBlocksCompact();

  void TrialClusterLegalization();
  void TetrisLegalizeCluster();

  double WireLengthCost(Cluster *cluster, int l, int r, int left_bound, int right_bound);
  void FindBestLocalOrder(std::vector<Block *> &res,
                          double &cost,
                          Cluster *cluster,
                          int l,
                          int r,
                          int left_bound,
                          int right_bound,
                          int range);
  void LocalReorderInCluster(Cluster *cluster, int range = 3);
  void LocalReorderAllClusters();

  void StartPlacement() override;

  void GenMatlabClusterTable(std::string const &name_of_file);
};

inline int Cluster::UsedSize() const {
  return used_size_;
}

inline void Cluster::SetUsedSize(int used_size) {
  used_size_ = used_size;
}

inline void Cluster::UseSpace(int width) {
  used_size_ += width;
}

inline void Cluster::SetLLX(int lx) {
  lx_ = lx;
}

inline void Cluster::SetURX(int ux) {
  lx_ = ux - width_;
}

inline int Cluster::LLX() const {
  return lx_;
}

inline int Cluster::URX() const {
  return lx_ + width_;
}

inline double Cluster::CenterX() const {
  return lx_ + width_ / 2.0;
}

inline void Cluster::SetWidth(int width) {
  width_ = width;
}

inline int Cluster::Width() const {
  return width_;
}

inline void Cluster::SetLLY(int ly) {
  ly_ = ly;
}

inline void Cluster::SetURY(int uy) {
  ly_ = uy - height_;
}

inline int Cluster::LLY() const {
  return ly_;
}

inline int Cluster::URY() const {
  return ly_ + height_;
}

inline double Cluster::CenterY() const {
  return ly_ + height_ / 2.0;
}

inline void Cluster::SetHeight(int height) {
  height_ = height;
}

inline int Cluster::Height() const {
  return height_;
}

inline void Cluster::SetLoc(int lx, int ly) {
  lx_ = lx;
  ly_ = ly;
}

inline int ClusterColumn::Width() const {
  return width_;
}

inline int ClusterColumn::LLX() const {
  return lx_;
}

inline int ClusterColumn::URX() const {
  return lx_ + width_;
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
