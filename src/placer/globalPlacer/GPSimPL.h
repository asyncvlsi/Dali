//
// Created by Yihang Yang on 8/4/2019.
//

#ifndef DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
#define DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_

#include <cfloat>

#include <queue>
#include <random>

#include "common/misc.h"
#include "placer/placer.h"
#include "GPSimPL/boxbin.h"
#include "GPSimPL/cellcutpoint.h"
#include "GPSimPL/gridbinindex.h"
#include "GPSimPL/gridbin.h"
#include "solver.h"

// declares a row-major sparse matrix type of double
typedef Eigen::SparseMatrix<double, Eigen::RowMajor> SpMat;

// A triplet is a simple object representing a non-zero entry as the triplet: row index, column index, value.
typedef Eigen::Triplet<double> T;

// A "doublet" is a simple object representing a non-zero entry as: column index, value, for a given row index.
typedef IndexVal D;

class GPSimPL : public Placer {
 protected:
  // cached data
  double HPWLX_new = 0;
  double HPWLX_old = DBL_MAX;
  bool HPWLX_converge = false;
  double HPWLY_new = 0;
  double HPWLY_old = DBL_MAX;
  bool HPWLY_converge = false;

  // to configure CG solver
  double cg_tolerance_ = 1e-11;
  int cg_iteration_max_num_ = 200;
  double error_x = DBL_MAX;
  double error_y = DBL_MAX;
  double cg_total_hpwl_ = 0;

  // to avoid divergence when calculating net weight
  double width_epsilon;
  double height_epsilon;

  // for look ahead legalization
  double HPWL_intra_linearSolver_precision = 0.01;
  int b2b_update_max_iteration = 100;
  double alpha = 0.00;
  int cur_iter_ = 0;
  int max_iter_ = 100;
  double lal_total_hpwl_ = 0;

  double HPWL_LAL_new = 0;
  double HPWL_LAL_old = DBL_MAX;
  bool HPWL_LAL_converge = false;
  double HPWL_inter_linearSolver_precision = 0.01;

  int number_of_cell_in_bin = 15;
  int net_ignore_threshold = 100;

  // weight adjust factor
  double adjust_factor = 1.5;
  double base_factor = 0;
  double decay_factor = 5;
 public:
  GPSimPL();
  GPSimPL(double aspectRatio, double fillingRate);

  unsigned int TotBlockNum() { return GetCircuit()->TotBlkCount(); }
  void SetEpsilon() {
    width_epsilon = circuit_->AveMovBlkWidth() / 100.0;
    height_epsilon = circuit_->AveMovBlkHeight() / 100.0;
  }
  double WidthEpsilon() const { return width_epsilon; }
  double HeightEpsilon() const { return height_epsilon; }

  std::vector<int> Ax_row_size;
  std::vector<int> Ay_row_size;
  std::vector<std::vector<D>> ADx;
  std::vector<std::vector<D>> ADy;

  Eigen::VectorXd vx, vy;
  Eigen::VectorXd bx, by;
  SpMat Ax;
  SpMat Ay;
  Eigen::VectorXd x_anchor, y_anchor;
  bool x_anchor_set = false;
  bool y_anchor_set = false;
  std::vector<T> coefficientsx;
  std::vector<T> coefficientsy;
  Eigen::ConjugateGradient<SpMat, Eigen::Lower|Eigen::Upper> cgx;
  Eigen::ConjugateGradient<SpMat, Eigen::Lower|Eigen::Upper> cgy;

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
  void CGInit();
  void InitCGFlags();
  void UpdateCGFlagsX();
  void UpdateHPWLX() { HPWLX_new = HPWLX(); }
  void UpdateMaxMinX();
  void UpdateMaxMinCtoCX();
  void UpdateCGFlagsY();
  void UpdateHPWLY() { HPWLY_new = HPWLY(); }
  void UpdateMaxMinY();
  void UpdateMaxMinCtoCY();
  void AddMatrixElement(Net &net, int i, int j, std::vector<T> &coefficients);
  void BuildProblemB2BX();
  void BuildProblemB2BY();
  void BuildProblemStarModelX();
  void BuildProblemStarModelY();
  void BuildProblemHPWLX();
  void BuildProblemHPWLY();
  void BuildProblemStarHPWLX();
  void BuildProblemStarHPWLY();
  void SolveProblemX();
  void SolveProblemY();
  void PullBlockBackToRegion();
  void InitialPlacement();

  // look ahead legalization member function implemented below
  int grid_bin_height;
  int grid_bin_width;
  int grid_cnt_y; // might distinguish the gird count in the x direction and y direction
  int grid_cnt_x;
  std::vector<std::vector<GridBin> > grid_bin_matrix;
  std::vector<std::vector<unsigned long int> > grid_bin_white_space_LUT;
  void InitGridBins();
  void InitWhiteSpaceLUT();
  unsigned long int LookUpWhiteSpace(GridBinIndex const &ll_index, GridBinIndex const &ur_index);
  unsigned long int LookUpWhiteSpace(WindowQuadruple &window);
  unsigned long int LookUpBlkArea(WindowQuadruple &window);
  unsigned long int WindowArea(WindowQuadruple &window);
  void LookAheadLgInit();
  void LookAheadClose();
  void ClearGridBinFlag();
  void UpdateGridBinState();

  std::vector<GridBinCluster> cluster_list;
  void ClusterOverfilledGridBin();
  void UpdateClusterArea();
  void UpdateClusterList();

  std::queue<BoxBin> queue_box_bin;
  static double BlkOverlapArea(Block *node1, Block *node2);
  void FindMinimumBoxForLargestCluster();
  void SplitBox(BoxBin &box);
  void SplitGridBox(BoxBin &box);
  void PlaceBlkInBox(BoxBin &box);
  void RoughLegalBlkInBox(BoxBin &box);
  void PlaceBlkInBoxBisection(BoxBin &box);
  bool RecursiveBisectionBlkSpreading();

  void BackUpBlkLoc();
  void LookAheadLegalization();
  void UpdateLALConvergeState();
  void UpdateAnchorLoc();
  void BuildProblemB2BWithAnchorX();
  void BuildProblemB2BWithAnchorY();
  void QuadraticPlacementWithAnchor();
  void UpdateAnchorNetWeight() {
    if (net_model==0) {
      alpha = 0.005 * cur_iter_;
    } else if (net_model==1) {
      alpha = 0.002 * cur_iter_;
    } else if (net_model==2) {
      alpha = 0.005 * cur_iter_;
    } else {
      alpha = 0.002 * cur_iter_;
    }
  }

  void CheckAndShift();

  double tot_lal_time = 0;
  double tot_cg_time = 0;
  bool StartPlacement() override;

  bool is_dump = false;
  void DumpResult(std::string const &name_of_file);
  void DrawBlockNetList(std::string const &name_of_file = "block_net_list.txt");
  void write_all_terminal_grid_bins(std::string const &name_of_file);
  void write_not_all_terminal_grid_bins(std::string const &name_of_file = "grid_bin_not_all_terminal.txt");
  void write_overfill_grid_bins(std::string const &name_of_file = "grid_bin_overfill.txt");
  void write_not_overfill_grid_bins(std::string const &name_of_file);
  void write_first_n_bin_cluster(std::string const &name_of_file, size_t n);
  void write_first_bin_cluster(std::string const &name_of_file);
  void write_n_bin_cluster(std::string const &name_of_file, size_t n);
  void write_all_bin_cluster(const std::string &name_of_file);
  void write_first_box(std::string const &name_of_file);
  void write_first_box_cell_bounding(std::string const &name_of_file);
};

#endif //DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
