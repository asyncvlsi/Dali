//
// Created by Yihang Yang on 8/4/2019.
//

#ifndef DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
#define DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_

#include <cfloat>

#include <queue>
#include <random>

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
  double cg_tolerance_ = 1e-6;
  int cg_iteration_max_num_ = 50;
  double error_x = DBL_MAX;
  double error_y = DBL_MAX;
  double cg_total_hpwl_ = 0;

  // to avoid divergence when calculating net weight
  double width_epsilon;
  double height_epsilon;

  // for look ahead legalization
  double HPWL_intra_linearSolver_precision = 0.01;
  int b2b_update_max_iteration = 50;
  double alpha = 0.00;
  int current_iteration_ = 0;
  int max_iteration_ = 50;
  double lal_total_hpwl_ = 0;

  double HPWL_LAL_new = 0;
  double HPWL_LAL_old = DBL_MAX;
  bool HPWL_LAL_converge = false;
  double HPWL_inter_linearSolver_precision = 0.01;

  int number_of_cell_in_bin = 30;
  int net_ignore_threshold = 100;
 public:
  GPSimPL();
  GPSimPL(double aspectRatio, double fillingRate);

  unsigned int TotBlockNum();
  void SetEpsilon();
  double WidthEpsilon();
  double HeightEpsilon();

  Eigen::VectorXd vx, vy;
  Eigen::VectorXd bx, by;
  SpMat Ax, Ay;
  std::vector<double> x_anchor, y_anchor;
  std::vector<T> coefficients;
  Eigen::ConjugateGradient<SpMat, Eigen::Lower> cgx;
  Eigen::ConjugateGradient<SpMat, Eigen::Lower> cgy;

  void BlockLocRandomInit();
  void BlockLocCenterInit();
  void CGInit();
  void InitCGFlags();
  void UpdateCGFlagsX();
  void UpdateHPWLX();
  void UpdateMaxMinX();
  void UpdateMaxMinCtoCX();
  void UpdateCGFlagsY();
  void UpdateHPWLY();
  void UpdateMaxMinY();
  void UpdateMaxMinCtoCY();
  void AddMatrixElement(Net &net, int i, int j);
  void BuildProblemB2B(bool is_x_direction, Eigen::VectorXd &b);
  void BuildProblemB2BX();
  void BuildProblemB2BY();
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
  void UpdateAnchorNetWeight();

  void CheckAndShift();

  void StartPlacement() override;

  void DumpResult();
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

inline unsigned int GPSimPL::TotBlockNum() {
  return GetCircuit()->TotBlockNum();
}

inline void GPSimPL::SetEpsilon() {
  width_epsilon = circuit_->AveMovWidth() / 100.0;
  height_epsilon = circuit_->AveMovHeight() / 100.0;
}

inline double GPSimPL::WidthEpsilon() {
  return width_epsilon;
}

inline double GPSimPL::HeightEpsilon() {
  return height_epsilon;
}

inline void GPSimPL::UpdateHPWLX() {
  HPWLX_new = HPWLX();
}

inline void GPSimPL::UpdateHPWLY() {
  HPWLY_new = HPWLY();
}

inline void GPSimPL::UpdateAnchorNetWeight() {
  alpha = 0.01 * current_iteration_;
}

#endif //DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
