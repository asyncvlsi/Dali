//
// Created by Yihang Yang on 8/4/2019.
//

#ifndef DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
#define DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_

#include <cfloat>

#include <queue>
#include <random>
#include <set>
#include <map>
#include <vector>

#include "dali/common/misc.h"
#include "dali/placer/placer.h"
#include "dali/solver.h"
#include "GPSimPL/boxbin.h"
#include "GPSimPL/cellcutpoint.h"
#include "GPSimPL/gridbinindex.h"
#include "GPSimPL/gridbin.h"

namespace dali {

// declares a row-major sparse matrix type of double
typedef Eigen::SparseMatrix<double, Eigen::RowMajor> SpMat;

// A triplet is a simple object representing a non-zero entry as the triplet: row index, column index, value.
typedef Eigen::Triplet<double> T;

// A "doublet" is a simple object representing a non-zero entry as: column index, value, for a given row index.
typedef IndexVal D;

class GPSimPL : public Placer {
 protected:
  /**** storing the lower and upper bound of hpwl along x and y direction ****/
  double init_hpwl_x_ = DBL_MAX;
  double init_hpwl_y_ = DBL_MAX;
  double init_hpwl_ = DBL_MAX;
  std::vector<double> lower_bound_hpwlx_;
  std::vector<double> lower_bound_hpwly_;
  std::vector<double> lower_bound_hpwl_;
  std::vector<double> upper_bound_hpwlx_;
  std::vector<double> upper_bound_hpwly_;
  std::vector<double> upper_bound_hpwl_;

  /**** parameters for CG solver optimization configuration ****/
  double cg_tolerance_ = 1e-35; // this is to make sure cg_tolerance is the same for different machines
  int cg_iteration_ = 10; // cg solver runs this amount of iterations to optimize the quadratic metric everytime
  int cg_iteration_max_num_ = 1000; // cg solver runs at most this amount of iterations to optimize the quadratic metric, this number should be adaptive to circuit size
  double cg_stop_criterion_ = 0.0025; // cg solver stops if the cost change is less than this value for 3 iterations
  double net_model_update_stop_criterion_ = 0.01; // stop update net model if the cost change is less than this value for 3 iterations

  /**** two small positive numbers used to avoid divergence when calculating net weights ****/
  double epsilon_factor_ = 1.5;
  double width_epsilon_; // this value is 1/epsilon_factor_ times the average movable cell width
  double height_epsilon_; // this value is 1/epsilon_factor_ times the average movable cell height

  /**** anchor weight ****/
  double alpha = 0.00; // pseudo-net weight additional factor for anchor pseudo-net
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
 public:
  GPSimPL();
  GPSimPL(double aspectRatio, double fillingRate);

  void LoadConf(std::string const &config_file) override;

  void SetEpsilon() {
    width_epsilon_ = circuit_->AveMovBlkWidth() * epsilon_factor_;
    height_epsilon_ = circuit_->AveMovBlkHeight() * epsilon_factor_;
  }

  double weight_modulation(double init_weight, double norm_distance, double center, double dispersion) {
    return init_weight / (1+exp((norm_distance-center)/dispersion));
  }

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

  double tot_triplets_time_x = 0;
  double tot_triplets_time_y = 0;
  double tot_matrix_from_triplets_x = 0;
  double tot_matrix_from_triplets_y = 0;
  double tot_cg_solver_time_x = 0;
  double tot_cg_solver_time_y = 0;
  double tot_loc_update_time_x = 0;
  double tot_loc_update_time_y = 0;

  int net_model = 0; // 0: b2b; 1: star; 2:HPWL; 3: StarHPWL
  void BlockLocRandomInit();
  void BlockLocCenterInit();
  void DriverLoadPairInit();
  void CGInit();
  void UpdateMaxMinX();
  void UpdateMaxMinY();
  void BuildProblemB2BX();
  void BuildProblemB2BY();
  void BuildProblemStarModelX();
  void BuildProblemStarModelY();
  void BuildProblemHPWLX();
  void BuildProblemHPWLY();
  void BuildProblemStarHPWLX();
  void BuildProblemStarHPWLY();
  double OptimizeQuadraticMetricX(double cg_stop_criterion);
  double OptimizeQuadraticMetricY(double cg_stop_criterion);
  void PullBlockBackToRegion();

  void BuildProblemX();
  void BuildProblemY();
  double QuadraticPlacement(double net_model_update_stop_criterion);

  // look ahead legalization member function implemented below
  int grid_bin_height;
  int grid_bin_width;
  int grid_cnt_y; // might distinguish the gird count in the x direction and y direction
  int grid_cnt_x;
  std::vector<std::vector<GridBin> > grid_bin_matrix;
  std::vector<std::vector<unsigned long int> > grid_bin_white_space_LUT;

  double UpdateGridBinState_time = 0;
  double ClusterOverfilledGridBin_time = 0;
  double UpdateClusterArea_time = 0;
  double UpdateClusterList_time = 0;
  double FindMinimumBoxForLargestCluster_time = 0;
  double RecursiveBisectionBlkSpreading_time = 0;

  void InitGridBins();
  void InitWhiteSpaceLUT();
  unsigned long int LookUpWhiteSpace(GridBinIndex const &ll_index, GridBinIndex const &ur_index);
  unsigned long int LookUpWhiteSpace(WindowQuadruple &window);
  unsigned long int LookUpBlkArea(WindowQuadruple &window);
  unsigned long int WindowArea(WindowQuadruple &window);
  void LALInit();
  void LALClose();
  void ClearGridBinFlag();
  void UpdateGridBinState();

  std::multiset<GridBinCluster, std::greater<>> cluster_set;
  void UpdateClusterArea(GridBinCluster &cluster) {
    cluster.total_cell_area = 0;
    cluster.total_white_space = 0;
    for (auto &index: cluster.bin_set) {
      cluster.total_cell_area += grid_bin_matrix[index.x][index.y].cell_area;
      cluster.total_white_space += grid_bin_matrix[index.x][index.y].white_space;
    }
  }
  void UpdateClusterList();

  std::queue<BoxBin> queue_box_bin;
  static double BlkOverlapArea(Block *node1, Block *node2);
  void UpdateLargestCluster();
  void FindMinimumBoxForLargestCluster();
  void SplitBox(BoxBin &box);
  void SplitGridBox(BoxBin &box);
  void PlaceBlkInBox(BoxBin &box);
  void RoughLegalBlkInBox(BoxBin &box);
  void PlaceBlkInBoxBisection(BoxBin &box);
  void UpdateGridBinBlocks(BoxBin &box);
  bool RecursiveBisectionBlkSpreading();

  void BackUpBlockLocation();
  double LookAheadLegalization();
  void UpdateAnchorLocation();
  void UpdateAnchorNetWeight();
  void BuildProblemWithAnchorX();
  void BuildProblemWithAnchorY();
  double QuadraticPlacementWithAnchor(double net_model_update_stop_criterion);
  void UpdateAnchorAlpha() {
    if (net_model == 0) {
      if (0 <= cur_iter_ && cur_iter_ < 5) {
        alpha_step = 0.005;
      } else if (cur_iter_ < 10) {
        alpha_step = 0.01;
      } else if (cur_iter_ < 15) {
        alpha_step = 0.02;
      } else {
        alpha_step = 0.03;
      }
      alpha += alpha_step;
    } else if (net_model == 1) {
      alpha = 0.002 * cur_iter_;
    } else if (net_model == 2) {
      alpha = 0.005 * cur_iter_;
    } else {
      alpha = 0.002 * cur_iter_;
    }
  }

  void CheckAndShift();

  bool IsPlacementConverge();

  double tot_lal_time = 0;
  double tot_cg_time = 0;
  bool StartPlacement() override;

  bool is_dump = false;
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

  static bool IsSeriesConverge(std::vector<double> &data, int window_size, double tolerance);
  static bool IsSeriesOscillate(std::vector<double> &data, int window_size);
};

}

#endif //DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
