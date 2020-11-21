//
// Created by Yihang Yang on 2019-08-07.
//

#include "boxbin.h"

#include <cmath>

namespace dali {

BoxBin::BoxBin() {
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

void BoxBin::update_all_terminal(std::vector<std::vector<GridBin> > &grid_bin_matrix) {
  GridBin *bin;
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      bin = &grid_bin_matrix[x][y];
      if (!bin->IsAllFixedBlk()) {
        all_terminal = false;
        return;
      }
    }
  }
  all_terminal = true;
}

void BoxBin::update_cell_area(std::vector<Block> &Nodelist) {
  /*
  int temp_total_cell_area = 0;
  Block *node;
  for (auto &cell_id: cell_list) {
    node = &Nodelist[cell_id];
    temp_total_cell_area += node->Area();
  }
  BOOST_LOG_TRIVIAL(info)   << "Total cell area: " << total_cell_area << "  " << temp_total_cell_area << "\n";
  temp_total_cell_area = 0;
  for (auto &node: Nodelist) {
    if (node.isterminal()) continue;
    if ((node.x0 >= ll_point.x) && (node.x0 < ur_point.x) && (node.y0 >= ll_point.y) && (node.y0 < ur_point.y)) {
      temp_total_cell_area += node.Area();
    }
  }
  BOOST_LOG_TRIVIAL(info)   << "Total cell area: " << total_cell_area << "  " << temp_total_cell_area << "\n";
  */

  Block *node;
  total_cell_area = 0;
  for (auto &cell_id: cell_list) {
    node = &Nodelist[cell_id];
    total_cell_area += node->Area();
  }
}

void BoxBin::update_cell_area_white_space(std::vector<std::vector<GridBin> > &grid_bin_matrix) {
  total_cell_area = 0;
  total_white_space = 0;
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      total_white_space += grid_bin_matrix[x][y].white_space;
      total_cell_area += grid_bin_matrix[x][y].cell_area;
    }
  }
  filling_rate = double(total_cell_area) / double(total_white_space);
}

void BoxBin::UpdateCellAreaWhiteSpaceFillingRate(std::vector<std::vector<unsigned long int>> &grid_bin_white_space_LUT,
                                                 std::vector<std::vector<GridBin> > &grid_bin_matrix) {
  if (ll_index.x == 0) {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y];
    } else {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ur_index.x][ll_index.y - 1];
    }
  } else {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ll_index.x - 1][ur_index.y];
    } else {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ur_index.x][ll_index.y - 1]
          - grid_bin_white_space_LUT[ll_index.x - 1][ur_index.y]
          + grid_bin_white_space_LUT[ll_index.x - 1][ll_index.y - 1];
    }
  }
  total_cell_area = 0;
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      total_cell_area += grid_bin_matrix[x][y].cell_area;
    }
  }
  filling_rate = double(total_cell_area) / double(total_white_space);
}

void BoxBin::ExpandBox(int grid_cnt_x, int grid_cnt_y) {
  if (ll_index.x > 0) --ll_index.x;
  if (ll_index.y > 0) --ll_index.y;
  if (ur_index.x < grid_cnt_x - 1) ++ur_index.x;
  if (ur_index.y < grid_cnt_y - 1) ++ur_index.y;
}

bool BoxBin::write_box_boundary(std::string const &NameOfFile) {
  std::ofstream ost;
  ost.open(NameOfFile.c_str(), std::ios::app);
  if (ost.is_open() == 0) {
    BOOST_LOG_TRIVIAL(info) << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  double low_x, low_y, width, height;
  width = right - left;
  height = top - bottom;
  low_x = left;
  low_y = bottom;
  int step = 20;
  for (int j = 0; j < height; j += step) {
    ost << low_x << "\t" << low_y + j << "\n";
    ost << low_x + width << "\t" << low_y + j << "\n";
  }
  for (int j = 0; j < width; j += step) {
    ost << low_x + j << "\t" << low_y << "\n";
    ost << low_x + j << "\t" << low_y + height << "\n";
  }
  ost.close();
  return true;
}

bool BoxBin::write_cell_region(std::string const &NameOfFile) {
  std::ofstream ost;
  ost.open(NameOfFile.c_str(), std::ios::app);
  if (ost.is_open() == 0) {
    BOOST_LOG_TRIVIAL(info) << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  double low_x, low_y, width, height;
  width = ur_point.x - ll_point.x;
  height = ur_point.y - ll_point.y;
  low_x = ll_point.x;
  low_y = ll_point.y;
  int step = 30;
  for (int j = 0; j < height; j += step) {
    ost << low_x << "\t" << low_y + j << "\n";
    ost << low_x + width << "\t" << low_y + j << "\n";
  }
  for (int j = 0; j < width; j += step) {
    ost << low_x + j << "\t" << low_y << "\n";
    ost << low_x + j << "\t" << low_y + height << "\n";
  }
  ost.close();
  return true;
}

void BoxBin::UpdateCellList(std::vector<std::vector<GridBin> > &grid_bin_matrix) {
  cell_list.clear();
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      for (auto &cell_id: grid_bin_matrix[x][y].cell_list) {
        cell_list.push_back(cell_id);
      }
    }
  }
}

void BoxBin::update_boundaries(std::vector<std::vector<GridBin> > &grid_bin_matrix) {
  left = grid_bin_matrix[ll_index.x][ll_index.y].left;
  bottom = grid_bin_matrix[ll_index.x][ll_index.y].bottom;
  right = grid_bin_matrix[ur_index.x][ur_index.y].right;;
  top = grid_bin_matrix[ur_index.x][ur_index.y].top;
}

void BoxBin::update_terminal_list_white_space(std::vector<Block> &Nodelist, std::vector<int> &box_terminal_list) {
  int node_llx, node_lly, node_urx, node_ury, bin_llx, bin_lly, bin_urx, bin_ury;
  int min_urx, max_llx, min_ury, max_lly;
  int overlap_x, overlap_y, overlap_area;
  bool not_overlap;
  Block *node;
  total_white_space = (right - left) * (top - bottom);
  for (auto &terminal_id: box_terminal_list) {
    node = &Nodelist[terminal_id];
    node_llx = (int) node->LLX();
    node_lly = (int) node->LLY();
    node_urx = (int) node->URX();
    node_ury = (int) node->URY();
    bin_llx = left;
    bin_lly = bottom;
    bin_urx = right;
    bin_ury = top;
    not_overlap = ((node_llx >= bin_urx) || (node_lly >= bin_ury)) || ((bin_llx >= node_urx) || (bin_lly >= node_ury));
    if (!not_overlap) {
      max_llx = std::max(node_llx, bin_llx);
      max_lly = std::max(node_lly, bin_lly);
      min_urx = std::min(node_urx, bin_urx);
      min_ury = std::min(node_ury, bin_ury);
      overlap_x = min_urx - max_llx;
      overlap_y = min_ury - max_lly;
      overlap_area = overlap_x * overlap_y;
      terminal_list.push_back(terminal_id);
      total_white_space -= overlap_area;
    }
    if (total_white_space < 1) {
      total_white_space = 0;
    }
  }
}

void BoxBin::UpdateObsBoundary(std::vector<Block> &block_list) {
  if (terminal_list.empty()) {
    return;
  }
  vertical_obstacle_boundaries.clear();
  horizontal_obstacle_boundaries.clear();
  Block *node;
  for (auto &terminal_id: terminal_list) {
    node = &block_list[terminal_id];
    if ((left < node->LLX()) && (right > node->LLX())) {
      vertical_obstacle_boundaries.push_back((int) node->LLX());
    }
    if ((left < node->URX()) && (right > node->URX())) {
      vertical_obstacle_boundaries.push_back((int) node->URX());
    }
    if ((bottom < node->LLY()) && (top > node->LLY())) {
      horizontal_obstacle_boundaries.push_back((int) node->LLY());
    }
    if ((bottom < node->URY()) && (top > node->URY())) {
      horizontal_obstacle_boundaries.push_back((int) node->URY());
    }
  }
  /* sort boundaries in the ascending order */
  size_t min_index;
  int min_boundary, tmp_boundary;
  if (!horizontal_obstacle_boundaries.empty()) {
    for (size_t i = 0; i < horizontal_obstacle_boundaries.size(); i++) {
      min_index = i;
      min_boundary = horizontal_obstacle_boundaries[i];
      for (size_t j = i + 1; j < horizontal_obstacle_boundaries.size(); j++) {
        if (horizontal_obstacle_boundaries[j] < min_boundary) {
          min_index = j;
          min_boundary = horizontal_obstacle_boundaries[j];
        }
      }
      tmp_boundary = horizontal_obstacle_boundaries[i];
      horizontal_obstacle_boundaries[i] = horizontal_obstacle_boundaries[min_index];
      horizontal_obstacle_boundaries[min_index] = tmp_boundary;
    }
  }
  if (!vertical_obstacle_boundaries.empty()) {
    for (size_t i = 0; i < vertical_obstacle_boundaries.size(); i++) {
      min_index = i;
      min_boundary = vertical_obstacle_boundaries[i];
      for (size_t j = i + 1; j < vertical_obstacle_boundaries.size(); j++) {
        if (vertical_obstacle_boundaries[j] < min_boundary) {
          min_index = j;
          min_boundary = vertical_obstacle_boundaries[j];
        }
      }
      tmp_boundary = vertical_obstacle_boundaries[i];
      vertical_obstacle_boundaries[i] = vertical_obstacle_boundaries[min_index];
      vertical_obstacle_boundaries[min_index] = tmp_boundary;
    }
  }
  /*
  BOOST_LOG_TRIVIAL(info)   << "Horizontal_obs_boundaries: ";
  for (auto &boundary: horizontal_obstacle_boundaries) {
    BOOST_LOG_TRIVIAL(info)   << boundary << " ";
  }
  BOOST_LOG_TRIVIAL(info)   << "\n";
  BOOST_LOG_TRIVIAL(info)   << "Vertical_obs_boundaries: ";
  for (auto &boundary: vertical_obstacle_boundaries) {
    BOOST_LOG_TRIVIAL(info)   << boundary << " ";
  }
  BOOST_LOG_TRIVIAL(info)   << "\n";
  */
}

bool BoxBin::write_cell_in_box(std::string const &NameOfFile, std::vector<Block> &Nodelist) {
  std::ofstream ost;
  ost.open(NameOfFile.c_str(), std::ios::app);
  if (ost.is_open() == 0) {
    BOOST_LOG_TRIVIAL(info) << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  for (auto &cell_id: cell_list) {
    if (Nodelist[cell_id].IsMovable()) {
      ost << Nodelist[cell_id].X() << "\t" << Nodelist[cell_id].Y() << "\n";
    }
  }
  ost.close();
  return true;
}

unsigned long int BoxBin::white_space_LUT(std::vector<std::vector<unsigned long int>> &grid_bin_white_space_LUT,
                                          GridBinIndex &ll,
                                          GridBinIndex &ur) {
  unsigned long int white_space;
  if (ll.x == 0) {
    if (ll.y == 0) {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y];
    } else {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y]
          - grid_bin_white_space_LUT[ur.x][ll.y - 1];
    }
  } else {
    if (ll.y == 0) {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y]
          - grid_bin_white_space_LUT[ll.x - 1][ur.y];
    } else {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y]
          - grid_bin_white_space_LUT[ur.x][ll.y - 1]
          - grid_bin_white_space_LUT[ll.x - 1][ur.y]
          + grid_bin_white_space_LUT[ll.x - 1][ll.y - 1];
    }
  }
  return white_space;
}

bool BoxBin::update_cut_index_white_space(std::vector<std::vector<unsigned long int>> &grid_bin_white_space_LUT) {
  double error, minimum_error = 1;
  unsigned long int white_space_low;
  int index_give_minimum_error;
  if (cut_direction_x) {
    if (ll_index.y == ur_index.y) return false;
    cut_ur_index.x = ur_index.x;
    cut_ll_index.x = ll_index.x;

    index_give_minimum_error = ll_index.y;

    for (cut_ur_index.y = ll_index.y; cut_ur_index.y < ur_index.y - 1; cut_ur_index.y++) {
      //BOOST_LOG_TRIVIAL(info)   << cut_ur_index.y << "\n";
      white_space_low = white_space_LUT(grid_bin_white_space_LUT, ll_index, cut_ur_index);
      error = std::fabs(double(white_space_low) / double(total_white_space) - 0.5);
      if (error < minimum_error) index_give_minimum_error = cut_ur_index.y;
      if (double(white_space_low) / double(total_white_space) > 0.5) break;
    }
    if (cut_ur_index.y != index_give_minimum_error) {
      cut_ur_index.y = index_give_minimum_error;
      //white_space_low = white_space_LUT(grid_bin_white_space_LUT, ll_index_, cut_ur_index);
    }
    cut_ll_index.y = cut_ur_index.y + 1;
    return true;
  } else {
    if (ll_index.x == ur_index.x) return false;
    cut_ur_index.y = ur_index.y;
    cut_ll_index.y = ll_index.y;

    index_give_minimum_error = ll_index.x;

    for (cut_ur_index.x = ll_index.x; cut_ur_index.x < ur_index.x - 1; cut_ur_index.x++) {
      //BOOST_LOG_TRIVIAL(info)   << cut_ur_index.x << "\n";
      white_space_low = white_space_LUT(grid_bin_white_space_LUT, ll_index, cut_ur_index);
      error = std::fabs(double(white_space_low) / double(total_white_space) - 0.5);
      if (error < minimum_error) index_give_minimum_error = cut_ur_index.x;
      if (double(white_space_low) / double(total_white_space) > 0.5) break;
    }
    if (cut_ur_index.x != index_give_minimum_error) {
      cut_ur_index.x = index_give_minimum_error;
      //white_space_low = white_space_LUT(grid_bin_white_space_LUT, ll_index_, cut_ur_index);
    }
    cut_ll_index.x = cut_ur_index.x + 1;
    return true;
  }
}

bool BoxBin::update_cut_point_cell_list_low_high(std::vector<Block> &Nodelist,
                                                 unsigned long int &box1_total_white_space,
                                                 unsigned long int &box2_total_white_space) {
  // this member function will be called only when two white spaces are not different from each other for several magnitudes
  unsigned long int cell_area_low = 0;
  double cut_line_low, cut_line_high;
  double cut_line = 0;
  double ratio = 1 + double(box2_total_white_space) / double(box1_total_white_space);
  //TODO: this method can be accelerated
  Block *node;
  if (cut_direction_x) {
    cut_ur_point.x = ur_point.x;
    cut_ll_point.x = ll_point.x;
    cut_line_low = ll_point.y;
    cut_line_high = ur_point.y;
    for (int i = 0; i < 20; i++) {
      //BOOST_LOG_TRIVIAL(info)   << i << "\n";
      cell_area_low = 0;
      cut_line = (cut_line_low + cut_line_high) / 2;
      for (auto &cell_id: cell_list) {
        node = &Nodelist[cell_id];
        if (node->Y() < cut_line) {
          cell_area_low += node->Area();
        }
      }
      //BOOST_LOG_TRIVIAL(info)   << cell_area_low/(double)total_cell_area << "\n";
      double tmp_ratio = double(total_cell_area) / double(cell_area_low);
      //TODO: this precision has some influence on the final result
      if (ratio > tmp_ratio) {
        cut_line_high = cut_line;
      } else if (ratio < tmp_ratio) {
        cut_line_low = cut_line;
      } else {
        break;
      }
    }
    total_cell_area_low = cell_area_low;
    total_cell_area_high = total_cell_area - total_cell_area_low;
    cut_ll_point.y = cut_line;
    cut_ur_point.y = cut_line;
    //BOOST_LOG_TRIVIAL(info)   << cut_line << " LLY " << ll_point.y << " URY " << ll_point.y << "\n";
    for (auto &cell_id: cell_list) {
      node = &Nodelist[cell_id];
      if (node->Y() < cut_line) {
        cell_list_low.push_back(cell_id);
      } else {
        cell_list_high.push_back(cell_id);
      }
    }
  } else {
    cut_ur_point.y = ur_point.y;
    cut_ll_point.y = ll_point.y;
    cut_line_low = ll_point.x;
    cut_line_high = ur_point.x;
    for (int i = 0; i < 20; i++) {
      //BOOST_LOG_TRIVIAL(info)   << i << "\n";
      cell_area_low = 0;
      cut_line = (cut_line_low + cut_line_high) / 2;
      for (auto &cell_id: cell_list) {
        node = &Nodelist[cell_id];
        if (node->X() < cut_line) {
          cell_area_low += node->Area();
        }
      }
      //BOOST_LOG_TRIVIAL(info)   << cell_area_low/(double)total_cell_area << "\n";
      double tmp_ratio = double(total_cell_area) / double(cell_area_low);
      if (ratio > tmp_ratio) {
        cut_line_high = cut_line;
      } else if (ratio < tmp_ratio) {
        cut_line_low = cut_line;
      } else {
        break;
      }
    }
    total_cell_area_low = cell_area_low;
    total_cell_area_high = total_cell_area - total_cell_area_low;
    cut_ll_point.x = cut_line;
    cut_ur_point.x = cut_line;
    //BOOST_LOG_TRIVIAL(info)   << cut_line << " LLX " << ll_point.x << " URX " << ur_point.x << "\n";
    for (auto &cell_id: cell_list) {
      node = &Nodelist[cell_id];
      if (node->X() < cut_line) {
        cell_list_low.push_back(cell_id);
      } else {
        cell_list_high.push_back(cell_id);
      }
    }
  }
  return true;
}

bool BoxBin::update_cut_point_cell_list_low_high_leaf(std::vector<Block> &Nodelist,
                                                      int &cut_line_w,
                                                      int ave_blk_height) {
  unsigned long int cell_area_low = 0;
  double cut_line = 0;
  // the above three cut-lines are for cells, thus they needs to be double
  double ratio = 2.0;
  // this ratio is to say that cell_area_low should be close to total_cell_area/ratio
  double mini_error = 1;
  double cell_area_low_percentage = 0;

  /* the way used here is sorting the cells instead of calculate the cut-line using bisection
   * when cut_direction_x is true, the new cut line of white space is chosen to be the middle of top and bottom, then update
   * cell_list_low and cell_list_high
   * when cut_direction_x is false, the new cut line of white space is chosen to be close to one half of total cell area */
  Block *node, *node1;
  if (cut_direction_x) {
    int box_height = top - bottom;
    int row_num = box_height / ave_blk_height;
    double low_white_space_total_ratio = 0.5;
    // first part, find the cut-line of white space, which the the middle of top and bottom of this box
    low_white_space_total_ratio = std::floor(row_num / 2.0) / row_num;
    cut_line_w = bottom + (int) (low_white_space_total_ratio * box_height);
    /*
    if ((box_height % ave_blk_height == 0) && (box_height > ave_blk_height)) {
      //BOOST_LOG_TRIVIAL(info)   << left << " " << right << " " << bottom << " " << top << "\n";
      low_white_space_total_ratio = std::floor(row_num/2.0)/row_num;
      cut_line_w = bottom + (int)(low_white_space_total_ratio * box_height);
    } else {
      BOOST_LOG_TRIVIAL(info)   << left << " " << right << " " << bottom << " " << top << "\n";
      BOOST_LOG_TRIVIAL(info)   << "Error: out of expectation, bin height is not the integer multiple of standard cell height!\n";
      exit(1);
    }*/

    /* second part, split the total cell_list to two part,
     * by sort cell_list based on y location in ascending order */
    size_t mini_index;
    int tmp_cell_id;
    double mini_loc;
    for (size_t i = 0; i < cell_list.size(); i++) {
      node = &Nodelist[cell_list[i]];
      mini_index = i;
      mini_loc = node->Y();
      for (size_t j = i + 1; j < cell_list.size(); j++) {
        node1 = &Nodelist[cell_list[j]];
        if (node1->Y() < mini_loc) {
          mini_index = j;
          mini_loc = node1->Y();
        }
      }
      tmp_cell_id = cell_list[mini_index];
      cell_list[mini_index] = cell_list[i];
      cell_list[i] = tmp_cell_id;
    }

    /* and then, find the index of cell, the total cell area below which is closest to one half of the total cell area */
    unsigned long int tmp_tot_cell_area_low = 0;
    int index_tot_cell_low_closest_to_half = 0;
    for (size_t i = 0; i < cell_list.size(); i++) {
      node = &Nodelist[cell_list[i]];
      tmp_tot_cell_area_low += node->Area();
      cell_area_low_percentage = double(tmp_tot_cell_area_low) / double(total_cell_area);
      //BOOST_LOG_TRIVIAL(info)   << i << " " << cell_area_low_percentage << "\n";
      if (fabs(cell_area_low_percentage - low_white_space_total_ratio) < mini_error) {
        mini_error = fabs(cell_area_low_percentage - low_white_space_total_ratio);
        index_tot_cell_low_closest_to_half = i;
      }
      if (cell_area_low_percentage >= low_white_space_total_ratio) {
        //BOOST_LOG_TRIVIAL(info)   << "mini_error: " << mini_error << " index: " << index_tot_cell_low_closest_to_half << "\n";
        break;
      }
    }
    /* if the index is smaller than this index, put the cell_id to cell_list_low, otherwise, put it to cell_list_high
     * and update total_cell_area_low and total_cell_area_high */
    cell_area_low = 0;
    for (int i = 0; i < (int) cell_list.size(); i++) {
      node = &Nodelist[cell_list[i]];
      if (i <= index_tot_cell_low_closest_to_half) {
        cell_area_low += node->Area();
        cell_list_low.push_back(cell_list[i]);
      } else {
        cell_list_high.push_back(cell_list[i]);
      }
    }
    total_cell_area_low = cell_area_low;
    total_cell_area_high = total_cell_area - total_cell_area_low;

    /* third part, find the cut-line to split cell area,
     * in this case, the absolute value of this line if actually not important,
     * it is set to be the y-coordinate of the cell closest to half of the total cell area */
    cut_ur_point.x = ur_point.x;
    cut_ll_point.x = ll_point.x;
    node = &Nodelist[cell_list[index_tot_cell_low_closest_to_half]];
    cut_line = node->Y();
    cut_ll_point.y = cut_line;
    cut_ur_point.y = cut_line;
  } else {
    /* first, split the total cell_list to two part,
     * by sort cell_list based on y location in ascending order */
    size_t mini_index;
    int tmp_cell_id;
    double mini_loc;
    for (size_t i = 0; i < cell_list.size(); i++) {
      node = &Nodelist[cell_list[i]];
      mini_index = i;
      mini_loc = node->X();
      for (size_t j = i + 1; j < cell_list.size(); j++) {
        node1 = &Nodelist[cell_list[j]];
        if (node1->X() < mini_loc) {
          mini_index = j;
          mini_loc = node1->X();
        }
      }
      tmp_cell_id = cell_list[mini_index];
      cell_list[mini_index] = cell_list[i];
      cell_list[i] = tmp_cell_id;
    }
    /* second, find the index of cell, the total cell area below which is closest to one half of the total cell area */
    unsigned long int tmp_tot_cell_area_low = 0;
    int index_tot_cell_low_closest_to_half = 0;
    for (size_t i = 0; i < cell_list.size(); i++) {
      node = &Nodelist[cell_list[i]];
      tmp_tot_cell_area_low += node->Area();
      cell_area_low_percentage = double(tmp_tot_cell_area_low) / double(total_cell_area);
      //BOOST_LOG_TRIVIAL(info)   << i << " " << cell_area_low_percentage << "\n";
      if (fabs(cell_area_low_percentage - 1 / ratio) < mini_error) {
        mini_error = fabs(cell_area_low_percentage - 1 / ratio);
        index_tot_cell_low_closest_to_half = i;
      }
      if (cell_area_low_percentage > 0.5) {
        //BOOST_LOG_TRIVIAL(info)   << "mini_error: " << mini_error << " index: " << index_tot_cell_low_closest_to_half << "\n";
        break;
      }
    }
    /* third, if the index is smaller than this index, put the cell_id to cell_list_low, otherwise, put it to cell_list_high
     * and update total_cell_area_low and total_cell_area_high */
    cell_area_low = 0;
    for (int i = 0; i < (int) cell_list.size(); i++) {
      node = &Nodelist[cell_list[i]];
      if (i <= index_tot_cell_low_closest_to_half) {
        cell_area_low += node->Area();
        cell_list_low.push_back(cell_list[i]);
      } else {
        cell_list_high.push_back(cell_list[i]);
      }
    }
    total_cell_area_low = cell_area_low;
    total_cell_area_high = total_cell_area - total_cell_area_low;
    /* forth, find the cut-line to split cell area,
     * in this case, the absolute value of this line if actually not important,
     * it is set to be the y-coordinate of the cell closest to half of the total cell area */
    cut_ur_point.y = ur_point.y;
    cut_ll_point.y = ll_point.y;
    node = &Nodelist[cell_list[index_tot_cell_low_closest_to_half]];
    cut_line = node->X();
    cut_ll_point.y = cut_line;
    cut_ur_point.y = cut_line;
    /* finally, the cut-line for white space is proportional to the total_cell_area_low */
    cut_line_w = left + (int) ((double(total_cell_area_low) / double(total_cell_area)) * (right - left));
  }
  return true;
}

}
