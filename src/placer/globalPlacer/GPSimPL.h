//
// Created by yihan on 8/4/2019.
//

#ifndef HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
#define HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_

#include <queue>
#include "placer/placer.h"
#include "circuit/bin.h"
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
 private:
  double HPWLX_new = 0;
  double HPWLY_new = 0;
  double HPWLX_old = 1e30;
  double HPWLY_old = 1e30;
  bool HPWLX_converge = false;
  bool HPWLY_converge = false;
  double cg_precision = 0.001;
  int cg_iteration_max_num = 20;
  double HPWL_intra_linearSolver_precision = 0.001;
 public:
  GPSimPL();
  GPSimPL(double aspectRatio, double fillingRate);
  int TotBlockNum();
  double WidthEpsilon();
  double HeightEpsilon();

  std::vector< double > x, y;
  std::vector< double > anchor_x, anchor_y;

  void BlockLocInit();
  void InitCGFlags();
  void UpdateCGFlagsX();
  void UpdateHPWLX();
  void UpdateMaxMinX();
  void UpdateMaxMinCtoCX();
  void UpdateCGFlagsY();
  void UpdateHPWLY();
  void UpdateMaxMinY();
  void UpdateMaxMinCtoCY();
  void BuildProblemB2B(bool is_x_direction, SpMat &A, Eigen::VectorXd &b);
  void QuadraticPlacement();

  int GRID_BIN_HEIGHT, GRID_BIN_WIDTH;
  int GRID_NUM;
  double alpha = 0;
  std::vector< std::vector<GridBin> > grid_bin_matrix;
  std::vector< std::vector<int> > grid_bin_white_space_LUT;
  void create_grid_bins();
  void init_update_white_space_LUT();
  void init_look_ahead_legal();
  int white_space(GridBinIndex const &ll_index, GridBinIndex const &ur_index);
  bool write_all_terminal_grid_bins(std::string const &NameOfFile);
  bool write_not_all_terminal_grid_bins(std::string const &NameOfFile);
  bool write_overfill_grid_bins(std::string const &NameOfFile);
  bool write_not_overfill_grid_bins(std::string const &NameOfFile);
  void update_grid_bins_state();

  std::vector< GridBinCluster > cluster_list;
  void cluster_sort_grid_bins();
  bool write_first_n_bin_cluster(std::string const &NameOfFile, size_t n);
  bool write_first_bin_cluster(std::string const &NameOfFile);
  bool write_all_bin_cluster(const std::string &NameOfFile);
  bool write_first_box(std::string const &NameOfFile);
  bool write_first_box_cell_bounding(std::string const &NameOfFile);

  std::queue < BoxBin > queue_box_bin;
  void find_box_first_cluster();
  void box_split(BoxBin &box);
  void grid_bin_box_split(BoxBin &box);
  void cell_placement_in_box(BoxBin &box);
  double cell_overlap(Block *node1, Block *node2);
  void cell_placement_in_box_molecular_dynamics(BoxBin &box);
  void cell_placement_in_box_bisection(BoxBin &box);
  bool recursive_bisection_cell_spreading();
  void look_ahead_legal();

  void copy_xy_to_anchor();
  void swap_xy_anchor();
  void initial_placement();
  void look_ahead_legalization();
  void linear_system_solve();
  void global_placement ();

  void DrawBlockNetList(std::string const &name_of_file= "block_net_list.txt");
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
