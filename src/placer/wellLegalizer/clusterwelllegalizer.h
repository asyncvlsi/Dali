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

  int well_extension;
  int plug_width;

  int p_well_height;

  int width;
  int height;
  int lx;
  int ly;
  std::vector<Block *> blk_ptr_list;

  // cached value;
  int modified_lx;

  int Width() const { return width; }
  int Height() const { return height; }
  int InnerUX() const { return modified_lx + width; }
  double CenterY() const { return ly + height / 2.0; }
  void AppendBlock(Block &block);
  void OptimizeHeight();
  void UpdateBlockLocation();
};

class ClusterWellLegalizer : public LGTetrisEx {
 private:
  int well_extension = 2;
  int plug_width = 4;
  int max_well_length = 40;

  double new_cluster_cost_threshold = 100;

  std::vector<BlockCluster *> row_to_cluster_;

  std::unordered_set<BlockCluster *> cluster_set;

 public:
  ClusterWellLegalizer();
  void InitializeClusterLegalizer();
  BlockCluster *CreateNewCluster();
  void AddBlockToCluster(Block &block, BlockCluster *cluster);
  BlockCluster *FindClusterForBlock(Block &block);
  void StartPlacement() override;

};

#endif //DALI_SRC_PLACER_WELLLEGALIZER_CLUSTERWELLLEGALIZER_H_
