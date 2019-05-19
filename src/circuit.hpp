//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef HPCC_CIRCUIT_HPP
#define HPCC_CIRCUIT_HPP

#include <vector>
#include <queue>
#include <set>
#include "circuit_node_net.hpp"
#include "circuit_bin.cpp"

typedef struct  {
  size_t pin;
  float weight;
} weight_tuple;
// weight tuple, include the pin number and the corresponding weight
// for more information about the sparse matrix format, see the documents

class circuit_t {
public:
  circuit_t();
  size_t CELL_NUM, TERMINAL_NUM;
  // total movable cells number and terminals number, calculated in ispd_read_write.hpp, used in global_placer.hpp, and conjugate_gradient.hpp
  int LEFT, RIGHT, BOTTOM, TOP;
  // Boundaries of the chip, calculated in global_placer.hpp, used in conjugate_gradient.hpp
  float AVE_CELL_AREA, AVE_WIDTH, AVE_HEIGHT, WEPSI, HEPSI;
  // WEPSI and HEPSI are added to avoid the divergence of weight. calculated in global_placer.hpp, used in conjugate_gradient.hpp
  float cg_precision;
  // this is the terminate condition for cg solver, if error is smaller than this precision, then stop
  //this variable is designed to decrease after each look-ahead legalization
  float HPWL_intra_linearSolver_precision;
  // this is the terminate condition for DeltaHPWL, in the algorithm, when deltaHWPL converges, stop execution
  float HPWLX_new, HPWLY_new, HPWLX_old, HPWLY_old;
  // these variables are defined to monitor the convergence of HPWL
  bool HPWLx_converge, HPWLy_converge;
  // boolean variable to indicate the converge of HPWLx and HPWLy to determine when to stop execution
  float HPWL_inter_linearSolver_precision;
  // HPWL and gap should converge, this is the criterion for the converge of HPWL, note that this is different
  // from the previous HPWL related criterion
  float TARGET_FILLING_RATE;
  // target density of cells, movable cells density in each bin cannot exceed this density constraint
  int GRID_NUM;
  int GRID_BIN_WIDTH, GRID_BIN_HEIGHT;
  // Initial grid numbers in both x direction and y direction
  float ALPHA;
  // the weight of the anchor link
  float BETA;
  // the grid size decreases by BETA at each iteration of global placement until it reaches 4x the average movable cell size

  std::vector< node_t > Nodelist;
  std::vector< net_t > Netlist;
  int std_cell_height;
  int real_LEFT, real_RIGHT, real_BOTTOM, real_TOP;
  void read_nodes_file(std::string const &NameOfFile);
  void read_pl_file(std::string const &NameOfFile);
  void read_nets_file(std::string const &NameOfFile);
  bool read_scl_file(std::string const &NameOfFile);
  bool write_pl_solution(std::string const &NameOfFile);
  bool write_pl_anchor_solution(std::string const &NameOfFile);
  bool write_node_terminal(std::string const &NameOfFile="terminal.txt", std::string const &NameOfFile1="nodes.txt");
  bool write_anchor_terminal(std::string const &NameOfFile="terminal.txt", std::string const &NameOfFile1="nodes.txt");
  /* implemented in circuit_t_io.cpp */

  std::vector< std::vector<weight_tuple> > Ax, Ay;
  // declare matrix here, such that every function in this namespace can see these matrix
  std::vector<size_t> kx, ky;
  // track the length of each row of Ax and Ay
  std::vector<float> bx, by;
  void cg_init();
  void cg_close();
  void initialize_HPWL_flags();
  void build_problem_clique_x();
  void build_problem_clique_y();
  void update_max_min_node_x();
  void update_HPWL_x();
  void build_problem_b2b_x();
  void build_problem_b2b_x_nooffset();
  void update_max_min_node_y();
  void update_HPWL_y();
  void build_problem_b2b_y();
  void build_problem_b2b_y_nooffset();
  void add_anchor_x();
  void add_anchor_y();
  void CG_solver(std::string const &dimension, std::vector< std::vector<weight_tuple> > &A, std::vector<float> &b, std::vector<size_t> &k);
  void CG_solver_x();
  void CG_solver_y();
  /* implemented in circuit_t_conjugate_gradient.cpp */

  void uniform_initialization();
  void initial_placement();

  std::vector< std::vector< grid_bin > > grid_bin_matrix;
  std::vector< std::vector< int > > grid_bin_white_space_LUT;
  void create_grid_bins();
  void init_update_white_space_LUT();
  int white_space(grid_bin_index const &ll_index, grid_bin_index const &ur_index);
  void update_grid_bins_state();
  bool write_all_terminal_grid_bins(std::string const &NameOfFile="grid_bin_all_terminal.txt");
  bool write_not_all_terminal_grid_bins(std::string const &NameOfFile="grid_bin_not_all_terminal.txt");
  bool write_overfill_grid_bins(std::string const &NameOfFile="grid_bin_overfill.txt");
  bool write_not_overfill_grid_bins(std::string const &NameOfFile="grid_bin_not_overfill.txt");

  std::vector< grid_bin_cluster > cluster_list;
  void cluster_sort_grid_bins();
  bool write_first_n_bin_cluster(std::string const &NameOfFile, size_t n);
  bool write_first_bin_cluster(std::string const &NameOfFile ="first_grid_bin_cluster.txt");
  bool write_all_bin_cluster(std::string const &NameOfFile="all_grid_bin_clusters.txt");

  std::queue < box_bin > queue_box_bin;
  void find_box_first_cluster();
  bool write_first_box(std::string const &NameOfFile="first_bounding_box.txt");
  bool write_first_box_cell_bounding(std::string const &NameOfFile="first_cell_bounding_box.txt");
  void grid_bin_box_split(box_bin &box);
  void box_split(box_bin &box);
  void cell_placement_in_box(box_bin &box);
  float cell_overlap(node_t *node1, node_t *node2);
  void cell_placement_in_box_molecular_dynamics(box_bin &box);
  void cell_placement_in_box_bisection(box_bin &box);
  bool recursive_bisection_cell_spreading();
  void init_look_ahead_legal();
  void look_ahead_legal();
  void look_ahead_legalization();
  void copy_xy_to_anchor();
  void swap_xy_anchor();
  /* implemented in circuit_t_look_ahead_legalization.cpp */
  void linear_system_solve();
  void global_placement();
  void global_placer();
};

#endif //HPCC_CIRCUIT_HPP
