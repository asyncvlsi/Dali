//
// Created by Yihang Yang on 8/4/2019.
//

#ifndef HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
#define HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_

#include <queue>
#include <random>
#include "../placer.h"
#include "../../../module/Eigen/Sparse"
#include "../../../module/Eigen//IterativeLinearSolvers"
#include "GPSimPL/simplblockaux.h"
#include "GPSimPL/gridbinindex.h"
#include "GPSimPL/gridbin.h"
#include "GPSimPL/boxbin.h"
#include "GPSimPL/cellcutpoint.h"

typedef Eigen::SparseMatrix<double, Eigen::RowMajor> SpMat; // declares a row-major sparse matrix type of double
typedef Eigen::Triplet<double> T; // A triplet is a simple object representing a non-zero entry as the triplet: row index, column index, value.

class GPSimPL: public Placer {
 protected:
  double HPWLX_new = 0;
  double HPWLY_new = 0;
  double HPWLX_old = 1e30;
  double HPWLY_old = 1e30;
  bool HPWLX_converge = false;
  bool HPWLY_converge = false;
  double cg_precision = 0.001;
  int cg_iteration_max_num = 30;
  double HPWL_intra_linearSolver_precision = 0.05;
  double alpha = 0.00;
  double alpha_increment = 0.01;
  int look_ahead_iter_max = 30;
  double HPWL_LAL_new = 0;
  double HPWL_LAL_old = 1e30;
  bool HPWL_LAL_converge = false;
  double HPWL_inter_linearSolver_precision = 0.05;
  int number_of_cell_in_bin = 30;
 public:
  GPSimPL();
  GPSimPL(double aspectRatio, double fillingRate);
  int TotBlockNum();
  double WidthEpsilon();
  double HeightEpsilon();

  std::minstd_rand0 generator{1};
  Eigen::VectorXd x, y;
  Eigen::VectorXd bx, by;
  SpMat Ax, Ay;
  std::vector< double > x_anchor, y_anchor;
  std::vector< T > coefficients;
  Eigen::BiCGSTAB  <SpMat, Eigen::IdentityPreconditioner> cgx;
  Eigen::BiCGSTAB  <SpMat, Eigen::IdentityPreconditioner> cgy;

  void BlockLocInit();
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
  void BuildProblemB2B(bool is_x_direction, Eigen::VectorXd &b);
  void BuildProblemB2BX();
  void BuildProblemB2BY();
  void SolveProblemX();
  void SolveProblemY();
  void InitialPlacement();

  // look ahead legalization member function implemented below
  int grid_bin_height;
  int grid_bin_width;
  int grid_cnt_y; // might distinguish the gird count in the x direction and y direction
  int grid_cnt_x;
  std::vector< std::vector<GridBin> > grid_bin_matrix;
  std::vector< std::vector<int> > grid_bin_white_space_LUT;
  void InitGridBins();
  void InitWhiteSpaceLUT();
  int LookUpWhiteSpace(GridBinIndex const &ll_index, GridBinIndex const &ur_index);
  void LookAheadLgInit();
  void LookAheadClose();
  void ClearGridBinFlag();
  void UpdateGridBinState();

  std::vector< GridBinCluster > cluster_list;
  void ClusterOverfilledGridBin();
  void SortGridBinCluster();
  void UpdateClusterList();

  std::queue < BoxBin > queue_box_bin;
  static double BlkOverlapArea(Block *node1, Block *node2);
  void FindMinimumBoxForFirstCluster();
  void SplitBox(BoxBin &box);
  void SplitGridBox(BoxBin &box);
  void PlaceBlkInBox(BoxBin &box);
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

  void StartPlacement() override;

  void DrawBlockNetList(std::string const &name_of_file= "block_net_list.txt");
  void write_all_terminal_grid_bins(std::string const &name_of_file);
  void write_not_all_terminal_grid_bins(std::string const &name_of_file);
  void write_overfill_grid_bins(std::string const &name_of_file);
  void write_not_overfill_grid_bins(std::string const &name_of_file);
  void write_first_n_bin_cluster(std::string const &name_of_file, size_t n);
  void write_first_bin_cluster(std::string const &name_of_file);
  void write_all_bin_cluster(const std::string &name_of_file);
  void write_first_box(std::string const &name_of_file);
  void write_first_box_cell_bounding(std::string const &name_of_file);
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
