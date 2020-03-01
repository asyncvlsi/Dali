//
// Created by Yihang Yang on 2/28/20.
//

#ifndef DALI_SRC_PLACER_WELLLEGALIZER_CLUSTERWELLLEGALIZER_H_
#define DALI_SRC_PLACER_WELLLEGALIZER_CLUSTERWELLLEGALIZER_H_

#include <unordered_set>
#include <vector>

#include "circuit/block.h"
#include "placer/legalizer/LGTetrisEx.h"

struct BlockCluster {
  BlockCluster(int well_extension_init, int plug_width_init);

  int well_extension_;
  int plug_width_;

  int p_well_height_;

  int width_;
  int height_;
  int lx_;
  int ly_;
  std::vector<Block *> blk_ptr_list_;

  // cached value;
  int modified_lx_;

  int Width() const { return width_; }
  int Height() const { return height_; }
  long int Area() const { return (long int) width_ * (long int) height_; }
  int InnerUX() const { return modified_lx_ + width_; }
  int LLX() const { return lx_; }
  int LLY() const { return ly_; }
  int URX() const { return lx_ + width_; }
  int URY() const { return ly_ + height_; }
  double CenterX() const { return lx_ + width_ / 2.0; }
  double CenterY() const { return ly_ + height_ / 2.0; }

  void SetLLX(int lx) { lx_ = lx; }
  void SetLLY(int ly) { ly_ = ly; }
  void SetURX(int ux) { lx_ = ux - width_; }
  void SetURY(int uy) { ly_ = uy - height_; }
  void SetLoc(int lx, int ly) {
    lx_ = lx;
    ly_ = ly;
  }
  void SetCenterX(double center_x) { lx_ = (int) std::round(center_x - width_ / 2.0); }
  void SetCenterY(double center_y) { ly_ = (int) std::round(center_y - height_ / 2.0); }

  void AppendBlock(Block &block);
  void OptimizeHeight();
  void UpdateBlockLocation();
};

struct CluPtrLocPair {
  BlockCluster *clus_ptr;
  int x;
  int y;
  explicit CluPtrLocPair(BlockCluster *clus_ptr_init = nullptr, int x_init = 0, int y_init = 0)
      : clus_ptr(clus_ptr_init), x(x_init), y(y_init) {}
  bool operator<(const CluPtrLocPair &rhs) const {
    return (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
  }
};

class ClusterWellLegalizer : public LGTetrisEx {
 private:
  int well_extension = 0;
  int plug_width = 4;
  int max_well_length = 40;

  double new_cluster_cost_threshold = 100;

  std::vector<BlockCluster *> row_to_cluster_;

  std::vector<CluPtrLocPair> cluster_loc_list_;
  std::unordered_set<BlockCluster *> cluster_set_;

 public:
  ClusterWellLegalizer();
  ~ClusterWellLegalizer() override;

  void InitializeClusterLegalizer();

  BlockCluster *CreateNewCluster();
  void AddBlockToCluster(Block &block, BlockCluster *cluster);
  BlockCluster *FindClusterForBlock(Block &block);
  void ClusterBlocks();

  void UseSpaceLeft(int end_x, int lo_row, int hi_row);
  bool LegalizeClusterLeft();
  void UseSpaceRight(int end_x, int lo_row, int hi_row);
  bool LegalizeClusterRight();
  bool LegalizeCluster();

  void UpdateBlockLocation();
  void StartPlacement() override;

  void GenMatlabClusterTable(std::string const &name_of_file);
};

#endif //DALI_SRC_PLACER_WELLLEGALIZER_CLUSTERWELLLEGALIZER_H_
