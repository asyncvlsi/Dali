//
// Created by Yihang Yang on 2/28/20.
//

#ifndef DALI_SRC_PLACER_WELLLEGALIZER_CLUSTERWELLLEGALIZER_H_
#define DALI_SRC_PLACER_WELLLEGALIZER_CLUSTERWELLLEGALIZER_H_

#include <unordered_set>
#include <vector>

#include "block_cluster.h"
#include "circuit/block.h"
#include "common/displaceviewer.h"
#include "placer/legalizer/LGTetrisEx.h"

class ClusterWellLegalizer : public LGTetrisEx {
 protected:
  int well_spacing_x = 0;
  int well_spacing_y = 0;
  int well_min_width = 0;
  int well_extension_x = 0;
  int well_extension_y = 0;
  int plug_width = 4;
  int max_well_length = 40;

  double new_cluster_cost_threshold = 40;
  int overlap_cost = 0;

  std::vector<BlkCluster *> row_to_cluster_;
  std::vector<BlkCluster *> col_to_cluster_;

  std::vector<CluPtrLocPair> cluster_loc_list_;
  std::unordered_set<BlkCluster *> cluster_set_;

  DisplaceViewer<int> *displace_viewer_ = nullptr;
 public:
  ClusterWellLegalizer();
  ~ClusterWellLegalizer() override;

  static int CostInitDisplacement(int x_new, int y_new, int x_old, int y_old);
  int CostLeftBottomBoundary(int x, int y);
  int CostRightTopBoundary(int x, int y);

  void InitializeClusterLegalizer();
  void InitDisplaceViewer(int sz);
  void UploadClusterXY();
  void UploadClusterUV();

  BlkCluster *CreateNewCluster();
  void AddBlockToCluster(Block &block, BlkCluster *cluster);
  BlkCluster *FindClusterForBlock(Block &block);
  void ClusterBlocks();

  void UseSpaceLeft(int end_x, int lo_row, int hi_row);
  bool LegalizeClusterLeft();
  void UseSpaceRight(int end_x, int lo_row, int hi_row);
  bool LegalizeClusterRight();

  void UseSpaceBottom(int end_y, int lo_row, int hi_row);
  bool FindLocBottom(Value2D<int> &loc, int width, int height) override;
  void FastShiftBottom(int failure_point);
  bool LegalizeClusterBottom();

  void UseSpaceTop(int end_y, int lo_row, int hi_row);
  bool FindLocTop(Value2D<int> &loc, int width, int height) override;
  bool LegalizeClusterTop();

  bool LegalizeCluster(int iteration);

  void UpdateBlockLocation();

  void BlockGlobalSwap();
  void BlockVerticalSwap();

  double WireLengthCost(BlkCluster *cluster, int l, int r);
  void FindBestPermutation(std::vector<Block *> &res, double &cost, BlkCluster *cluster, int l, int r, int range);
  void LocalReorderInCluster(BlkCluster *cluster, int range = 3);
  void LocalReorderAllClusters();

  void StartPlacement() override;

  void GenMatlabClusterTable(std::string const &name_of_file);
  void ReportWellRule();
};

inline int ClusterWellLegalizer::CostInitDisplacement(int x_new, int y_new, int x_old, int y_old) {
  return std::abs(x_new - x_old) + std::abs(y_new - y_old);
}

inline int ClusterWellLegalizer::CostLeftBottomBoundary(int x, int y) {
  return std::abs(x - left_) + std::abs(y - bottom_);
}

inline int ClusterWellLegalizer::CostRightTopBoundary(int x, int y) {
  return std::abs(right_ - x) + std::abs(top_ - y);
}

#endif //DALI_SRC_PLACER_WELLLEGALIZER_CLUSTERWELLLEGALIZER_H_
