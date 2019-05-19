//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef HPCC_CIRCUIT_BIN_HPP
#define HPCC_CIRCUIT_BIN_HPP

#include <set>
#include <vector>

class grid_bin_index {
public:
  grid_bin_index();
  grid_bin_index(size_t x0, size_t y0);
  size_t x;
  size_t y;
  void init() {x = 0; y = 0;};
  bool operator <(const grid_bin_index &rhs) const;
  bool operator >(const grid_bin_index &rhs) const;
  bool operator ==(const grid_bin_index &rhs) const;
  friend std::ostream& operator<<(std::ostream& os, const grid_bin_index &index)
  {
    os << "(" << index.x << ", " << index.y << ")";
    return os;
  }
};

grid_bin_index::grid_bin_index() {
  x = 0;
  y = 0;
}

grid_bin_index::grid_bin_index(size_t x0, size_t y0) {
  x = x0;
  y = y0;
}

class grid_bin_cluster {
public:
  grid_bin_cluster();
  float total_cell_area;
  float total_white_space;
  std::set< grid_bin_index > grid_bin_index_set;
};

grid_bin_cluster::grid_bin_cluster() {
  total_cell_area = 0;
  total_white_space = 0;
}

class grid_bin {
public:
  grid_bin();
  grid_bin_index index;
  int bottom;
  int top;
  int left;
  int right;
  int area;
  int white_space;
  int cell_area;
  float filling_rate;
  bool all_terminal;
  bool over_fill; // a grid bin is over-filled, if filling rate is larger than the target, or cells locate on terminals
  bool cluster_visited;
  bool global_placed;
  std::vector< size_t > cell_list;
  std::vector< size_t > terminal_list;
  std::vector< grid_bin_index > adjacent_bin_index;
  int llx() { return left; }
  int lly() { return bottom; }
  int urx() { return right; }
  int ury() { return top; }
  bool is_all_terminal() { return all_terminal; }
  bool is_over_fill() { return over_fill; }
  void create_adjacent_bin_list(int GRID_NUM);
};

grid_bin::grid_bin() {
  index.init();
  bottom =0;
  top = 0;
  left = 0;
  right = 0;
  area = 0;
  white_space = 0;
  cell_area = 0;
  filling_rate = 0;
  all_terminal = false;
  over_fill = false;
  cluster_visited = false;
  global_placed = false;
}

class cell_cut_point {
public:
  cell_cut_point();
  cell_cut_point(float x0, float y0);
  float x;
  float y;
  void init() {x = 0; y = 0;};
  bool operator <(const cell_cut_point &rhs) const;
  bool operator >(const cell_cut_point &rhs) const;
  bool operator ==(const cell_cut_point &rhs) const;
  friend std::ostream& operator<<(std::ostream& os, const cell_cut_point &cut_point)
  {
    os << "(" << cut_point.x << ", " << cut_point.y << ")";
    return os;
  }
};

cell_cut_point::cell_cut_point() {
  x = 0;
  y = 0;
}

cell_cut_point::cell_cut_point(float x0, float y0) {
  x = x0;
  y = y0;
}

class box_bin {
public:
  box_bin();
  bool cut_direction_x;
  // cut line is alone x direction
  bool all_terminal;
  int total_white_space;
  float filling_rate;
  bool is_all_terminal() { return all_terminal; };
  /* Cut-line to split box white space */
  grid_bin_index ll_index;
  grid_bin_index ur_index;
  grid_bin_index cut_ll_index;
  grid_bin_index cut_ur_index;
  /* Cut-line to split cell area */
  cell_cut_point ll_point;
  cell_cut_point ur_point;
  cell_cut_point cut_ll_point;
  cell_cut_point cut_ur_point;
  /* total cell area, and the value in two children box */
  int total_cell_area;
  int total_cell_area_low;
  int total_cell_area_high;

  /* all cell_id in the box, and cell_id in two children box */
  std::vector< size_t > cell_list;
  std::vector< size_t > cell_list_low;
  std::vector< size_t > cell_list_high;

  /* the cell_id for terminals in the box, will be updated only when the box is a grid_bin
   * if there is no terminal in the grid bin, do not have to further split the box into smaller boxs,
   * otherwise split the box into smaller boxes, until there is no terminals in any boxes*/
  std::vector< size_t > terminal_list;
  void update_terminal_list(std::vector<std::vector<grid_bin> > &grid_bin_matrix) {terminal_list = grid_bin_matrix[ll_index.x][ll_index.y].terminal_list;};
  /* update_terminal_list can only be called when the box is a grid_bin_box */
  bool has_terminal() { return !terminal_list.empty(); };

  std::vector< int > vertical_obstacle_boundaries;
  std::vector< int > horizontal_obstacle_boundaries;
  void update_ob_boundaries(std::vector< node_t > &Nodelist);

  /* the boundary of box if the box is smaller than a grid bin, the following for attribute will be very important
   * the left, right, bottom, top boundaries are the region where cells will be placed in */
  int left;
  int right;
  int bottom;
  int top;
  void update_boundaries(std::vector<std::vector<grid_bin> > &grid_bin_matrix);
  /* update_white_space can only be called when left, right, bottom, top are updated */
  void update_terminal_list_white_space(std::vector<node_t> &Nodelist, std::vector< size_t > &box_terminal_list);

  void update_all_terminal(std::vector< std::vector< grid_bin > > &grid_bin_matrix);
  void update_cell_area(std::vector<node_t> &Nodelist);
  void update_cell_area_white_space(std::vector< std::vector< grid_bin > > &grid_bin_matrix);
  void update_cell_area_white_space_LUT(std::vector< std::vector< int > > &grid_bin_white_space_LUT, std::vector<std::vector<grid_bin> > &grid_bin_matrix);
  void expand_box(int GRID_NUM);
  bool write_box_boundary(std::string const &NameOfFile);
  bool write_cell_region(std::string const &NameOfFile="first_cell_bounding_box.txt");
  double white_space_LUT(std::vector< std::vector< int > > &grid_bin_white_space_LUT, grid_bin_index &ll, grid_bin_index &ur);
  void update_cell_in_box(std::vector<std::vector<grid_bin> > &grid_bin_matrix);
  bool write_cell_in_box(std::string const &NameOfFile, std::vector<node_t> &Nodelist);
  bool update_cut_index_white_space(std::vector< std::vector< int > > &grid_bin_white_space_LUT);
  bool update_cut_point_cell_list_low_high(std::vector<node_t> &Nodelist, int &box1_total_white_space, int &box2_total_white_space);
  bool update_cut_point_cell_list_low_high_leaf(std::vector<node_t> &Nodelist, int &cut_line_w, int std_cell_height);
};

box_bin::box_bin() {
  all_terminal = false;
  cut_direction_x = false;
  total_cell_area = 0;
  total_white_space = 0;
  filling_rate = 0;
  total_cell_area_low = 0;
  total_cell_area_high = 0;
  left = 0;
  right = 0;
  bottom = 0;
  top = 0;
}

#endif //HPCC_CIRCUIT_BIN_HPP
