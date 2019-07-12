//
// Created by yihan on 7/11/2019.
//

#ifndef HPCC_BOXBIN_H
#define HPCC_BOXBIN_H

#include <set>
#include <vector>
#include <iostream>
#include <fstream>
#include "grid_bin_index.h"
#include "cell_cut_point.h"
#include "grid_bin.h"
#include "AL/blockal.h"

class box_bin {
public:
  box_bin();
  bool cut_direction_x;
  // cut line is alone x direction
  bool all_terminal;
  int total_white_space;
  double filling_rate;
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
  void update_ob_boundaries(std::vector<block_al_t> &Nodelist);

  /* the boundary of box if the box is smaller than a grid bin, the following for attribute will be very important
   * the left, right, bottom, top boundaries are the region where cells will be placed in */
  int left;
  int right;
  int bottom;
  int top;
  void update_boundaries(std::vector<std::vector<grid_bin> > &grid_bin_matrix);
  /* update_white_space can only be called when left, right, bottom, top are updated */
  void update_terminal_list_white_space(std::vector<block_al_t> &Nodelist, std::vector< size_t > &box_terminal_list);

  void update_all_terminal(std::vector< std::vector< grid_bin > > &grid_bin_matrix);
  void update_cell_area(std::vector<block_al_t> &Nodelist);
  void update_cell_area_white_space(std::vector< std::vector< grid_bin > > &grid_bin_matrix);
  void update_cell_area_white_space_LUT(std::vector< std::vector< int > > &grid_bin_white_space_LUT, std::vector<std::vector<grid_bin> > &grid_bin_matrix);
  void expand_box(int GRID_NUM);
  bool write_box_boundary(std::string const &NameOfFile);
  bool write_cell_region(std::string const &NameOfFile="first_cell_bounding_box.txt");
  double white_space_LUT(std::vector< std::vector< int > > &grid_bin_white_space_LUT, grid_bin_index &ll, grid_bin_index &ur);
  void update_cell_in_box(std::vector<std::vector<grid_bin> > &grid_bin_matrix);
  bool write_cell_in_box(std::string const &NameOfFile, std::vector<block_al_t> &Nodelist);
  bool update_cut_index_white_space(std::vector< std::vector< int > > &grid_bin_white_space_LUT);
  bool update_cut_point_cell_list_low_high(std::vector<block_al_t> &Nodelist, int &box1_total_white_space, int &box2_total_white_space);
  bool update_cut_point_cell_list_low_high_leaf(std::vector<block_al_t> &Nodelist, int &cut_line_w, int std_cell_height);
};


#endif //HPCC_BOXBIN_H
