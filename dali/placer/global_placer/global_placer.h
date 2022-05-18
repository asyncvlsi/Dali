/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#ifndef DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_
#define DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_
#include <cfloat>

#include <queue>
#include <random>
#include <set>
#include <map>
#include <vector>

#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Sparse>

#include "dali/circuit/blkpairnets.h"
#include "dali/common/misc.h"
#include "dali/placer/placer.h"

#include "box_bin.h"
#include "cell_cut_point.h"
#include "grid_bin_index.h"
#include "grid_bin.h"

namespace dali {

typedef Eigen::Index EgId;
typedef std::pair<EgId, EgId> PairEgId;
// declares a row-major sparse matrix type of double
typedef Eigen::SparseMatrix<double, Eigen::RowMajor> SpMat;
// A triplet is a simple object representing a non-zero entry as the triplet: row index, column index, value.
typedef Eigen::Triplet<double> T;
// A "doublet" is a simple object representing a non-zero entry as: column index, value, for a given row index.
typedef IndexVal D;

class GlobalPlacer : public Placer {
 public:
  GlobalPlacer() = default;

  void SetMaxIteration(int max_iter);
  void LoadConf(std::string const &config_file) override;
  void UpdateEpsilon();
  void BlockLocationUniformInitialization();
  void BlockLocationNormalInitialization(double std_dev = 1.0 / 3.0);

  void InitializeConjugateGradientLinearSolver();
  void UpdateMaxMinX();
  void UpdateMaxMinY();

  // Bound2Bound net model
  void BuildProblemB2BX();
  void BuildProblemB2BY();

  // start net model
  void DecomposeNetsToBlkPairs();
  void InitializeDriverLoadPairs();
  void BuildProblemStarModelX();
  void BuildProblemStarModelY();

  // HPWL net model
  void BuildProblemHPWLX();
  void BuildProblemHPWLY();

  // star-HPWL net model
  void BuildProblemStarHPWLX();
  void BuildProblemStarHPWLY();

  double OptimizeQuadraticMetricX(double cg_stop_criterion);
  double OptimizeQuadraticMetricY(double cg_stop_criterion);
  void PullBlockBackToRegion();

  void BuildProblemX();
  void BuildProblemY();
  double QuadraticPlacement(double net_model_update_stop_criterion);

  void InitializeGridBinSize();
  void UpdateAttributesForAllGridBins();
  void UpdateFixedBlocksInGridBins();
  void UpdateWhiteSpaceInGridBin(GridBin &grid_bin);
  void InitGridBins();
  void InitWhiteSpaceLUT();
  unsigned long int LookUpWhiteSpace(
      GridBinIndex const &ll_index,
      GridBinIndex const &ur_index
  );
  unsigned long int LookUpWhiteSpace(WindowQuadruple &window);
  unsigned long int LookUpBlkArea(WindowQuadruple &window);
  unsigned long int WindowArea(WindowQuadruple &window);
  void LALInit();
  void LALClose();
  void ClearGridBinFlag();
  void UpdateGridBinState();

  void UpdateClusterArea(GridBinCluster &cluster);
  void UpdateClusterList();

  static double BlkOverlapArea(Block *node1, Block *node2);
  void UpdateLargestCluster();
  void FindMinimumBoxForLargestCluster();
  void SplitBox(BoxBin &box);
  void SplitGridBox(BoxBin &box);
  void PlaceBlkInBox(BoxBin &box);
  void RoughLegalBlkInBox(BoxBin &box);
  void PlaceBlkInBoxBisection(BoxBin &box);
  void UpdateGridBinBlocks(BoxBin &box);
  bool RecursiveBisectionblockspreading();

  void BackUpBlockLocation();
  double LookAheadLegalization();
  void UpdateAnchorLocation();
  void UpdateAnchorNetWeight();
  void BuildProblemWithAnchorX();
  void BuildProblemWithAnchorY();
  double QuadraticPlacementWithAnchor(double net_model_update_stop_criterion);
  void UpdateAnchorAlpha();
  bool IsPlacementConverge();

  bool StartPlacement() override;

  void SetDump(bool s_dump);
  void DumpResult(std::string const &name_of_file);
  void DumpLookAheadDisplacement(std::string const &base_name, int mode);
  void DrawBlockNetList(std::string const &name_of_file = "block_net_list.txt");
  void write_all_terminal_grid_bins(std::string const &name_of_file);
  void write_not_all_terminal_grid_bins(std::string const &name_of_file = "grid_bin_not_all_terminal.txt");
  void write_overfill_grid_bins(std::string const &name_of_file = "grid_bin_overfill.txt");
  void write_not_overfill_grid_bins(std::string const &name_of_file);
  void write_first_n_bin_cluster(std::string const &name_of_file, size_t n);
  void write_first_bin_cluster(std::string const &name_of_file);
  void write_all_bin_cluster(const std::string &name_of_file);
  void write_first_box(std::string const &name_of_file);
  void write_first_box_cell_bounding(std::string const &name_of_file);

  static bool IsSeriesConverge(
      std::vector<double> &data,
      int window_size,
      double tolerance
  );
  static bool IsSeriesOscillate(std::vector<double> &data, int window_size);
 protected:
  /**** storing the lower and upper bound of hpwl along x and y direction ****/
  std::vector<double> lower_bound_hpwlx_;
  std::vector<double> lower_bound_hpwly_;
  std::vector<double> lower_bound_hpwl_;
  std::vector<double> upper_bound_hpwlx_;
  std::vector<double> upper_bound_hpwly_;
  std::vector<double> upper_bound_hpwl_;

  /**** parameters for CG solver optimization configuration ****/
  // this is to make sure cg_tolerance is the same for different machines
  double cg_tolerance_ = 1e-35;
  // cg solver runs this amount of iterations to optimize the quadratic metric everytime
  int cg_iteration_ = 10;
  // cg solver runs at most this amount of iterations to optimize the quadratic metric, this number should be adaptive to circuit size
  int cg_iteration_max_num_ = 1000;
  // cg solver stops if the cost change is less than this value for 3 iterations
  double cg_stop_criterion_ = 0.0025;
  // stop update net model if the cost change is less than this value for 3 iterations
  double net_model_update_stop_criterion_ = 0.01;

  /**** two small positive numbers used to avoid divergence when calculating net weights ****/
  double epsilon_factor_ = 1.5;
  // this value will be set to 1/epsilon_factor_ times the average movable cell width
  double width_epsilon_ = 1e-5;
  // this value will be set to 1/epsilon_factor_ times the average movable cell height
  double height_epsilon_ = 1e-5;

  /**** anchor weight ****/
  // pseudo-net weight additional factor for anchor pseudo-net
  double alpha = 0.00;
  double alpha_step = 0.00;

  // for look ahead legalization
  int b2b_update_max_iteration_ = 50;
  int cur_iter_ = 0;
  int max_iter_ = 100;
  int number_of_cell_in_bin_ = 30;
  int net_ignore_threshold_ = 100;
  double simpl_LAL_converge_criterion_ = 0.005;
  double polar_converge_criterion_ = 0.08;
  int convergence_criteria_ = 1;

  // weight adjust factor
  double adjust_factor = 1.5;
  double base_factor = 0.0;
  double decay_factor = 2;

  // lal parameters
  int cluster_upper_size = 3;

  // look ahead legalization member function implemented below
  int grid_bin_height = 0;
  int grid_bin_width = 0;
  int grid_cnt_x = 0;
  int grid_cnt_y = 0;
  std::vector<std::vector<GridBin> > grid_bin_mesh;
  std::vector<std::vector<unsigned long long> > grid_bin_white_space_LUT;

  double UpdateGridBinState_time = 0;
  double ClusterOverfilledGridBin_time = 0;
  double UpdateClusterArea_time = 0;
  double UpdateClusterList_time = 0;
  double FindMinimumBoxForLargestCluster_time = 0;
  double RecursiveBisectionblockspreading_time = 0;

  std::vector<int> Ax_row_size;
  std::vector<int> Ay_row_size;
  std::vector<std::vector<D>> ADx;
  std::vector<std::vector<D>> ADy;

  Eigen::VectorXd vx, vy;
  Eigen::VectorXd bx, by;
  SpMat Ax;
  SpMat Ay;
  Eigen::VectorXd x_anchor, y_anchor;
  Eigen::VectorXd x_anchor_weight, y_anchor_weight;
  bool x_anchor_set = false;
  bool y_anchor_set = false;
  std::vector<T> coefficientsx;
  std::vector<T> coefficientsy;
  Eigen::ConjugateGradient<SpMat, Eigen::Lower | Eigen::Upper> cgx;
  Eigen::ConjugateGradient<SpMat, Eigen::Lower | Eigen::Upper> cgy;
  std::vector<std::vector<BlkPairNets *>> pair_connect;
  std::vector<BlkPairNets> diagonal_pair;
  std::vector<SpMat::InnerIterator> SpMat_diag_x;
  std::vector<SpMat::InnerIterator> SpMat_diag_y;

  // lower triangle of the driver-load pair
  std::vector<BlkPairNets> blk_pair_net_list_;
  std::unordered_map<PairEgId, EgId, boost::hash<PairEgId>> blk_pair_map_;

  int net_model = 0; // 0: b2b; 1: star; 2:HPWL; 3: StarHPWL
  std::multiset<GridBinCluster, std::greater<>> cluster_set;
  std::queue<BoxBin> queue_box_bin;
  bool is_dump_ = false;

  double tot_triplets_time_x = 0;
  double tot_triplets_time_y = 0;
  double tot_matrix_from_triplets_x = 0;
  double tot_matrix_from_triplets_y = 0;
  double tot_cg_solver_time_x = 0;
  double tot_cg_solver_time_y = 0;
  double tot_loc_update_time_x = 0;
  double tot_loc_update_time_y = 0;
  double tot_lal_time = 0;
  double tot_cg_time = 0;

  void BlockLocationUniformInitialization_();
  void BlockLocationNormalInitialization_(double std_dev);
  void BlockLocationInitialization_(int mode, double std_dev);
};

}

#endif //DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_
