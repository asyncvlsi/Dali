//
// Created by Yihang Yang on 2019-03-26.
//
#include <queue>
#include <set>
#include <algorithm>
#include "circuit.hpp"

void circuit_t::create_grid_bins() {
  /* this is a function which create the grid bins, based on the GRID_NUM and the boundaries of chip region, LEFT, RIGHT, BOTTOM, TOP
   * these four boundaries are given by the bounding box of placed macros/terminals now, these value should be specified in the .scl file
   * but it seems for global placement, the accurate value does not really matter now
   * in the future, when we want to do more careful global placement, we will then make some modifications*/

  /* determine the width and height of grid bin based on the boundaries and given GRID_NUM */

  GRID_BIN_HEIGHT = 8 * std_cell_height;
  GRID_NUM = std::ceil((float)(TOP - BOTTOM) / GRID_BIN_HEIGHT);
  GRID_BIN_WIDTH = std::ceil((float)(RIGHT - LEFT) / GRID_NUM);

  //std::cout << "GRID_BIN_HEIGHT: " << GRID_BIN_HEIGHT << "\n";
  //std::cout << "GRID_BIN_WIDTH: " << GRID_BIN_WIDTH << "\n";
  //std::cout << "GRID_NUM: " << GRID_NUM << "\n";

  /* create an empty grid bin column with size GRID_NUM and push the grid bin column to grid_bin_matrix for GRID_NUM times */
  std::vector<grid_bin> temp_grid_bin_column((size_t)GRID_NUM);
  for (int i = 0; i < GRID_NUM; i++) {
    grid_bin_matrix.push_back(temp_grid_bin_column);
  }

  /* for each grid bin, we need to initialize the attributes, including index, boundaries, area, and potential available white space
   * the adjacent bin list is created for the convenience of overfilled bin clustering */
  for (size_t i=0; i<grid_bin_matrix.size(); i++) {
    for (size_t j = 0; j < grid_bin_matrix[i].size(); j++) {
      grid_bin_matrix[i][j].index.x = i;
      grid_bin_matrix[i][j].index.y = j;
      grid_bin_matrix[i][j].bottom = BOTTOM + j * GRID_BIN_HEIGHT;
      grid_bin_matrix[i][j].top = BOTTOM + (j+1) * GRID_BIN_HEIGHT;
      grid_bin_matrix[i][j].left = LEFT + i * GRID_BIN_WIDTH;
      grid_bin_matrix[i][j].right = LEFT + (i+1) * GRID_BIN_WIDTH;
      grid_bin_matrix[i][j].area = (grid_bin_matrix[i][j].right - grid_bin_matrix[i][j].left) * (grid_bin_matrix[i][j].top - grid_bin_matrix[i][j].bottom);
      grid_bin_matrix[i][j].white_space = grid_bin_matrix[i][j].area; // at the very beginning, assuming the white space is the same as area
      grid_bin_matrix[i][j].create_adjacent_bin_list(GRID_NUM);
    }
  }

  for (size_t i=0; i<grid_bin_matrix.size(); i++) {
    grid_bin_matrix[i][GRID_NUM - 1].top = TOP;
  }
  for (size_t j=0; j<grid_bin_matrix.size(); j++) {
    grid_bin_matrix[GRID_NUM - 1][j].right = RIGHT;
  }

  /* check whether the grid bin is occupied by terminals, and we only need to check it once, if we do not want to change grid_bin configurations in the future
   * the idea of the part is that for each macro/terminal, we can easily calculate which grid bins it overlap with, and for each grid bin it overlap with,
   * we will check whether this grid bin is covered by this terminal, if yes, we set the label of this grid bin "all_terminal" to be true, initially, which is false
   * if not, we deduce the white space by the amount of terminal/macro and grid bin overlap region, because a grid bin might be covered by more than one terminals/macros
   * if the final white space is 0, we know the grid bin is also cover by terminals, and we set the flag "all_terminal" to be true also. */
  int node_llx, node_lly, node_urx, node_ury, bin_llx, bin_lly, bin_urx, bin_ury;
  int min_urx, max_llx, min_ury, max_lly;
  int overlap_x, overlap_y, overlap_area;
  bool all_terminal, terminal_out_of_region, terminal_out_of_box;
  int left_index, right_index, bottom_index, top_index;
  for (size_t i=CELL_NUM; i<Nodelist.size(); i++) {
    /* find the left, right, bottom, top index of the grid */
    terminal_out_of_region = ((int)Nodelist[i].llx() >= RIGHT) || ((int)Nodelist[i].urx() <= LEFT) || ((int)Nodelist[i].lly() >= TOP) || ((int)Nodelist[i].ury() <= BOTTOM);
    if (terminal_out_of_region) {
      continue;
    }
    left_index = (int)std::floor((Nodelist[i].llx() - LEFT)/GRID_BIN_WIDTH);
    right_index = (int)std::floor((Nodelist[i].urx() - LEFT)/GRID_BIN_WIDTH);
    bottom_index = (int)std::floor((Nodelist[i].lly() - BOTTOM)/GRID_BIN_HEIGHT);
    top_index = (int)std::floor((Nodelist[i].ury() - BOTTOM)/GRID_BIN_HEIGHT);
    /* sometimes the grid boundaries might be the placement region boundaries, need some fix here
     * because the top boundary and the right boundary of a grid bin does not actually belong to that grid bin */
    if (left_index < 0) {
      left_index = 0;
    }
    if (right_index == GRID_NUM) {
      right_index = GRID_NUM - 1;
    }
    if (bottom_index < 0) {
      bottom_index = 0;
    }
    if (top_index == GRID_NUM) {
      top_index = GRID_NUM - 1;
    }

    /* for each terminal, we will check which grid is inside it, and directly set the all_terminal attribute to true for that grid
     * some small terminals might occupy the same grid, we need to deduct the overlap area from the white space of that grid bin
     * when the final white space is 0, we know this grid bin is occupied by several terminals*/
    for (int j=left_index; j<=right_index; j++) {
      for (int k=bottom_index; k<=top_index; k++) {
        /* this is kind of rare case, the top/right of a terminal overlap with the bottom/left of a grid box
         * if this happens, we need to ignore this terminal for this grid box. */
        terminal_out_of_box = ((int)Nodelist[i].llx() >= grid_bin_matrix[j][k].right) || ((int)Nodelist[i].urx() <= grid_bin_matrix[j][k].left) ||
                                 ((int)Nodelist[i].lly() >= grid_bin_matrix[j][k].top) || ((int)Nodelist[i].ury() <= grid_bin_matrix[j][k].bottom);
        if (terminal_out_of_box) {
          continue;
        }
        grid_bin_matrix[j][k].terminal_list.push_back(i);

        /* if grid bin is covered by a large terminal, then this grid is all_terminal for sure */
        all_terminal = ((int)Nodelist[i].llx() <= grid_bin_matrix[j][k].llx()) && ((int)Nodelist[i].lly() <= grid_bin_matrix[j][k].lly()) &&
                       ((int)Nodelist[i].urx() >= grid_bin_matrix[j][k].urx()) && ((int)Nodelist[i].ury() >= grid_bin_matrix[j][k].ury());
        grid_bin_matrix[j][k].all_terminal = all_terminal;
        /* if not, we need to calculate the overlap of grid bin and this terminal to calculate white space, when white space is 0, this grid bin is also all_terminal */
        if (all_terminal) {
          grid_bin_matrix[j][k].white_space = 0;
        } else {
          node_llx = (int)Nodelist[i].llx();
          node_lly = (int)Nodelist[i].lly();
          node_urx = (int)Nodelist[i].urx();
          node_ury = (int)Nodelist[i].ury();
          bin_llx = grid_bin_matrix[j][k].llx();
          bin_lly = grid_bin_matrix[j][k].lly();
          bin_urx = grid_bin_matrix[j][k].urx();
          bin_ury = grid_bin_matrix[j][k].ury();

          max_llx = std::max(node_llx, bin_llx);
          max_lly = std::max(node_lly, bin_lly);
          min_urx = std::min(node_urx, bin_urx);
          min_ury = std::min(node_ury, bin_ury);

          overlap_x = min_urx - max_llx;
          overlap_y = min_ury - max_lly;
          overlap_area = overlap_x * overlap_y;
          grid_bin_matrix[j][k].white_space -= overlap_area;
          if (grid_bin_matrix[j][k].white_space < 1) {
            grid_bin_matrix[j][k].all_terminal = true;
            grid_bin_matrix[j][k].white_space = 0;
          }
        }
      }
    }
  }
}

void circuit_t::init_update_white_space_LUT() {
  /* this is a member function to initialize white space look-up table
   * this table is a matrix, one way to calculate the white space in a region is to add all white space of every single grid bin in it
   * but an easier way is to define an accumulate function and store it as a look-up table
   * when we want to find the white space in a region, just read it from the look-up table*/

  /* this for loop is created to initialize the size of the loop-up table */
  std::vector< int > tmp_vector(GRID_NUM);
  for (int i=0; i<GRID_NUM ; i++) {
    grid_bin_white_space_LUT.push_back(tmp_vector);
  }

  /* this for loop is used for computing elements in the look-up table
   * there are four cases, element at (0,0), elements on the left edge, elements on the right edge, otherwise*/
  for (size_t x=0; x<grid_bin_matrix.size(); x++) {
    for (size_t y=0; y<grid_bin_matrix[x].size(); y++) {
      grid_bin_white_space_LUT[x][y] = 0;
      if (x==0) {
        if (y==0) {
          grid_bin_white_space_LUT[x][y] = grid_bin_matrix[0][0].white_space;
        } else {
          grid_bin_white_space_LUT[x][y] = grid_bin_white_space_LUT[x][y-1] + grid_bin_matrix[x][y].white_space;
        }
      } else {
        if (y==0) {
          grid_bin_white_space_LUT[x][y] = grid_bin_white_space_LUT[x-1][y] + grid_bin_matrix[x][y].white_space;
        } else {
          grid_bin_white_space_LUT[x][y] = grid_bin_white_space_LUT[x-1][y] + grid_bin_white_space_LUT[x][y-1]
              + grid_bin_matrix[x][y].white_space - grid_bin_white_space_LUT[x-1][y-1];
        }
      }
    }
  }
}

void circuit_t::init_look_ahead_legal() {
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      bin.global_placed = false;
    }
  }
}

int circuit_t::white_space(grid_bin_index const &ll_index, grid_bin_index const &ur_index) {
  /* this function is used to return the white space in a region specified by ll_index, and ur_index
   * there are four cases, element at (0,0), elements on the left edge, elements on the right edge, otherwise */

  int total_white_space;
  if (ll_index.x == 0) {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y];
    } else {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
                          - grid_bin_white_space_LUT[ur_index.x][ll_index.y-1];
    }
  } else {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
                          - grid_bin_white_space_LUT[ll_index.x-1][ur_index.y];
    } else {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
                          - grid_bin_white_space_LUT[ur_index.x][ll_index.y-1]
                          - grid_bin_white_space_LUT[ll_index.x-1][ur_index.y]
                          + grid_bin_white_space_LUT[ll_index.x-1][ll_index.y-1];
    }
  }
  return total_white_space;
}

bool circuit_t::write_all_terminal_grid_bins(std::string const &NameOfFile) {
  /* this is a member function for testing, print grid bins where the flag "all_terminal" is true */
  std::ofstream ost(NameOfFile.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 3;
      float step_x = width/N, step_y = height/N;
      if (bin.is_all_terminal()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j*step_y << "\n";
          ost << low_x + width << "\t" << low_y + j*step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j*step_x << "\t" << low_y << "\n";
          ost << low_x + j*step_x << "\t" << low_y + height << "\n";
        }
      }
    }
  }
  ost.close();
  return true;
}

bool circuit_t::write_not_all_terminal_grid_bins(std::string const &NameOfFile) {
  /* this is a member function for testing, print grid bins where the flag "all_terminal" is false */
  std::ofstream ost(NameOfFile.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 3;
      float step_x = width/N, step_y = height/N;
      if (!bin.is_all_terminal()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j*step_y << "\n";
          ost << low_x + width << "\t" << low_y + j*step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j*step_x << "\t" << low_y << "\n";
          ost << low_x + j*step_x << "\t" << low_y + height << "\n";
        }
      }
    }
  }
  ost.close();
  return true;
}

bool circuit_t::write_overfill_grid_bins(std::string const &NameOfFile) {
  /* this is a member function for testing, print grid bins where the flag "over_fill" is true */
  std::ofstream ost(NameOfFile.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 3;
      float step_x = width/N, step_y = height/N;
      if (bin.is_over_fill()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j*step_y << "\n";
          ost << low_x + width << "\t" << low_y + j*step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j*step_x << "\t" << low_y << "\n";
          ost << low_x + j*step_x << "\t" << low_y + height << "\n";
        }
      }
    }
  }
  ost.close();
  return true;
}

bool circuit_t::write_not_overfill_grid_bins(std::string const &NameOfFile) {
  /* this is a member function for testing, print grid bins where the flag "over_fill" is false */
  std::ofstream ost(NameOfFile.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 3;
      float step_x = width/N, step_y = height/N;
      if (!bin.is_over_fill()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j*step_y << "\n";
          ost << low_x + width << "\t" << low_y + j*step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j*step_x << "\t" << low_y << "\n";
          ost << low_x + j*step_x << "\t" << low_y + height << "\n";
        }
      }
    }
  }
  ost.close();
  return true;
}

void circuit_t::update_grid_bins_state() {
  /* this is a member function to initialize grid bins, because the cell_list, cell_area and over_fill state can be changed
   * so we need to update them when necessary*/

  /* clean the old data */
  for (auto &&bin_column:grid_bin_matrix) {
    for (auto &&bin:bin_column) {
      bin.cell_list.clear();
      bin.cell_area = 0;
      bin.over_fill = false;
    }
  }

  /* for each cell, find the index of the grid bin it should be in
   * note that in extreme cases, the index might be smaller than 0 or larger than the maximum allowed index
   * because the cell is on the boundaries, so we need to make some modifications for these extreme cases*/
  int x_index, y_index;
  for (size_t i=0; i<CELL_NUM; i++) {
    x_index = (int)std::floor((Nodelist[i].x0 - LEFT)/GRID_BIN_WIDTH);
    y_index = (int)std::floor((Nodelist[i].y0 - BOTTOM)/GRID_BIN_HEIGHT);
    if (x_index < 0) {
      x_index = 0;
    }
    if (x_index > GRID_NUM-1) {
      x_index = GRID_NUM - 1;
    }
    if (y_index < 0) {
      y_index = 0;
    }
    if (y_index > GRID_NUM-1) {
      y_index = GRID_NUM - 1;
    }
    grid_bin_matrix[x_index][y_index].cell_list.push_back(i);
    grid_bin_matrix[x_index][y_index].cell_area += Nodelist[i].area();
  }

  /* below is the criterion to decide whether a grid bin is over_filled or not
   * 1. if this bin if fully occupied by terminals, but its cell_list is non-empty, which means there is some cells overlap with this grid bin, we say it is over_fill
   * 2. if not fully occupied by terminals, but filling_rate is larger than the TARGET_FILLING_RATE, then set is to over_fill
   * 3. if this bin is not overfilled, but cells in this bin overlaps with terminals in this bin, we also mark it as over_fill*/
  bool over_fill = false;
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      if (bin.global_placed) {
        bin.over_fill = false;
        continue;
      }
      if (bin.is_all_terminal()) {
        if (!bin.cell_list.empty()) {
          bin.over_fill = true;
        }
      } else {
        bin.filling_rate = (float) bin.cell_area / bin.white_space;
        if (bin.filling_rate > TARGET_FILLING_RATE) {
          bin.over_fill = true;
        }
      }
      if (!bin.is_over_fill()) {
        for (auto &&cell_num: bin.cell_list) {
          for (auto &&terminal_num: bin.terminal_list) {
            over_fill = Nodelist[cell_num].is_overlap(Nodelist[terminal_num]);
            if (over_fill) {
              bin.over_fill = true;
              break;
            }
          }
          if (over_fill) break;
          // two breaks have to be used to break two loops
        }
      }
    }
  }
}

void circuit_t::cluster_sort_grid_bins() {
  /* this function is to cluster over_fill grid bins, and sort them based on total cell area
   * the algorithm to cluster over_fill grid bins is breadth first search*/
  cluster_list.clear();
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      bin.cluster_visited = false;
    }
  }
  grid_bin_index tmp_index;
  for (size_t i=0; i<grid_bin_matrix.size(); i++) {
    for (size_t j=0; j<grid_bin_matrix[i].size(); j++) {
      if ((grid_bin_matrix[i][j].cluster_visited)||(!grid_bin_matrix[i][j].over_fill)) {
        grid_bin_matrix[i][j].cluster_visited = true;
        continue;
      }
      tmp_index.x = i;
      tmp_index.y = j;
      grid_bin_cluster H;
      H.grid_bin_index_set.insert(tmp_index);
      grid_bin_matrix[i][j].cluster_visited = true;
      std::queue < grid_bin_index > Q;
      Q.push(tmp_index);
      while (!Q.empty()) {
        tmp_index = Q.front();
        Q.pop();
        for (auto &&index: grid_bin_matrix[tmp_index.x][tmp_index.y].adjacent_bin_index) {
          grid_bin *bin = &grid_bin_matrix[index.x][index.y];
          if ((!bin->cluster_visited)&&(bin->over_fill)) {
            bin->cluster_visited = true;
            H.grid_bin_index_set.insert(index);
            Q.push(index);
          }
        }
      }
      cluster_list.push_back(H);
    }
  }

  /* sort grid bin clusters based on total cell area */
  /* first calculate the total cell area, total white space */
  for (auto &&cluster: cluster_list) {
    cluster.total_cell_area = 0;
    cluster.total_white_space = 0;
    for (auto &&index: cluster.grid_bin_index_set) {
      grid_bin *tmp_grid_bin = &grid_bin_matrix[index.x][index.y];
      cluster.total_cell_area += tmp_grid_bin->cell_area;
      cluster.total_white_space += tmp_grid_bin->white_space;
    }
    //std::cout << "Total cell area: " << cluster.total_cell_area;
    //std::cout << " Total white space: " << cluster.total_white_space << "\n";
  }
  /* then do the Selection sort */
  size_t max_index;
  grid_bin_cluster tmp_cluster;
  for (size_t i=0; i<cluster_list.size(); i++) {
    max_index = i;
    for (size_t j=i+1; j<cluster_list.size(); j++) {
      if (cluster_list[j].total_cell_area > cluster_list[max_index].total_cell_area) {
        max_index = j;
      }
    }
    tmp_cluster = cluster_list[i];
    cluster_list[i] = cluster_list[max_index];
    cluster_list[max_index] = tmp_cluster;
    //std::cout << "Total cell area: " << cluster_list[i].total_cell_area;
    //std::cout << " Total white space: " << cluster_list[i].total_white_space << "\n";
  }
}

bool circuit_t::write_first_n_bin_cluster(std::string const &NameOfFile, size_t n) {
  /* this is a member function for testing, print the first n over_filled clusters */
  std::ofstream ost(NameOfFile.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  for (size_t i=0; i<n; i++) {
    for (auto &&index: cluster_list[i].grid_bin_index_set) {
      double low_x, low_y, width, height;
      grid_bin *grid_bin = &grid_bin_matrix[index.x][index.y];
      width = grid_bin->right - grid_bin->left;
      height = grid_bin->top - grid_bin->bottom;
      low_x = grid_bin->left;
      low_y = grid_bin->bottom;
      int step = 40;
      if (grid_bin->is_over_fill()) {
        for (int j = 0; j < height; j += step) {
          ost << low_x << "\t" << low_y + j << "\n";
          ost << low_x + width << "\t" << low_y + j << "\n";
        }
        for (int j = 0; j < width; j += step) {
          ost << low_x + j << "\t" << low_y << "\n";
          ost << low_x + j << "\t" << low_y + height << "\n";
        }
      }
    }
  }
  ost.close();
  return true;
}

bool circuit_t::write_first_bin_cluster(std::string const &NameOfFile) {
  /* this is a member function for testing, print the first one over_filled clusters */
  return write_first_n_bin_cluster(NameOfFile, 1);
}

bool circuit_t::write_all_bin_cluster(const std::string &NameOfFile) {
  /* this is a member function for testing, print all over_filled clusters */
  return write_first_n_bin_cluster(NameOfFile, cluster_list.size());
}

bool circuit_t::write_first_box(std::string const &NameOfFile) {
  /* this is a member function for testing, print the first n over_filled clusters */
  std::ofstream ost(NameOfFile.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  double low_x, low_y, width, height;
  box_bin *R = &queue_box_bin.front();
  width = (R->ur_index.x - R->ll_index.x + 1) * GRID_BIN_WIDTH;
  height = (R->ur_index.y - R->ll_index.y + 1) * GRID_BIN_HEIGHT;
  low_x = R->ll_index.x * GRID_BIN_WIDTH + LEFT;
  low_y = R->ll_index.y * GRID_BIN_HEIGHT + BOTTOM;
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

bool circuit_t::write_first_box_cell_bounding(std::string const &NameOfFile) {
  /* this is a member function for testing, print the bounding box of cells in which all cells should be placed into corresponding boxes */
  std::ofstream ost(NameOfFile.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  double low_x, low_y, width, height;
  box_bin *R = &queue_box_bin.front();
  width = R->ur_point.x - R->ll_point.x;
  height = R->ur_point.y - R->ll_point.y;
  low_x = R->ll_point.x;
  low_y = R->ll_point.y;
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

void circuit_t::find_box_first_cluster() {
  /* this function find the box for the first cluster, such that the total white space in the box is larger than the total
   * cell area, the way to do this is just by expanding the boundaries of the bounding box of the first cluster */

  while(!queue_box_bin.empty()) queue_box_bin.pop();
  // clear the queue_box_bin

  if (cluster_list.empty()) return;
  // this is redundant, but for safety reasons. Probably this statement can be safely removed

  box_bin R;
  R.cut_direction_x = false;
  R.ll_index.x = GRID_NUM - 1;
  R.ll_index.y = GRID_NUM - 1;
  R.ur_index.x = 0;
  R.ur_index.y = 0;
  // initialize a box with y cut-direction
  // identify the bounding box of the initial cluster
  for (auto &&index: cluster_list[0].grid_bin_index_set) {
    grid_bin *bin = &grid_bin_matrix[index.x][index.y];
    if (bin->index.x < R.ll_index.x) {
      R.ll_index.x = bin->index.x;
    }
    if (bin->index.x > R.ur_index.x) {
      R.ur_index.x = bin->index.x;
    }
    if (bin->index.y < R.ll_index.y) {
      R.ll_index.y = bin->index.y;
    }
    if (bin->index.y > R.ur_index.y) {
      R.ur_index.y = bin->index.y;
    }
  }
  while (true) {
    // update cell area, white space, and thus filling rate to determine whether to expand this box or not
    R.total_white_space = white_space(R.ll_index, R.ur_index);
    R.update_cell_area_white_space_LUT(grid_bin_white_space_LUT, grid_bin_matrix);
    //R.update_cell_area_white_space(grid_bin_matrix);
    if (R.filling_rate > TARGET_FILLING_RATE) {
      R.expand_box(GRID_NUM);
    }
    else {
      break;
    }
  }
  R.update_cell_in_box(grid_bin_matrix);
  R.ll_point.x = grid_bin_matrix[R.ll_index.x][R.ll_index.y].left;
  R.ll_point.y = grid_bin_matrix[R.ll_index.x][R.ll_index.y].bottom;
  R.ur_point.x = grid_bin_matrix[R.ur_index.x][R.ur_index.y].right;
  R.ur_point.y = grid_bin_matrix[R.ur_index.x][R.ur_index.y].top;

  R.left = R.ll_point.x;
  R.bottom = R.ll_point.y;
  R.right = R.ur_point.x;
  R.top = R.ur_point.y;

  if (R.ll_index == R.ur_index) {
    R.update_terminal_list(grid_bin_matrix);
    if (R.has_terminal()) {
      R.update_ob_boundaries(Nodelist);
    }
  }
  queue_box_bin.push(R);
  //std::cout << "Bounding box total white space: " << queue_box_bin.front().total_white_space << "\n";
  //std::cout << "Bounding box total cell area: " << queue_box_bin.front().total_cell_area << "\n";

  for (size_t x = R.ll_index.x; x <= R.ur_index.x; x++) {
    for (size_t y = R.ll_index.y; y <= R.ur_index.y; y++) {
      grid_bin_matrix[x][y].global_placed = true;
    }
  }
}

void circuit_t::box_split(box_bin &box) {
  bool flag_bisection_complete;
  int dominating_box_flag; // indicate whether there is a dominating box_bin
  box_bin box1, box2;
  box1.ll_index = box.ll_index;
  box2.ur_index = box.ur_index;
  // this part of code can be simplified, but after which the code might be unclear
  // cut-line along vertical direction
  if (box.cut_direction_x) {
    flag_bisection_complete = box.update_cut_index_white_space(grid_bin_white_space_LUT);
    if (flag_bisection_complete) {
      box1.cut_direction_x = false;
      box2.cut_direction_x = false;
      box1.ur_index = box.cut_ur_index;
      box2.ll_index = box.cut_ll_index;
    } else {
      // if bisection fail in one direction, do bisection in the other direction
      box.cut_direction_x = false;
      flag_bisection_complete = box.update_cut_index_white_space(grid_bin_white_space_LUT);
      if (flag_bisection_complete) {
        box1.cut_direction_x = false;
        box2.cut_direction_x = false;
        box1.ur_index = box.cut_ur_index;
        box2.ll_index = box.cut_ll_index;
      }
    }
  } else {
    // cut-line along horizontal direction
    flag_bisection_complete = box.update_cut_index_white_space(grid_bin_white_space_LUT);
    if (flag_bisection_complete) {
      box1.cut_direction_x = true;
      box2.cut_direction_x = true;
      box1.ur_index = box.cut_ur_index;
      box2.ll_index = box.cut_ll_index;
    } else {
      box.cut_direction_x = true;
      flag_bisection_complete = box.update_cut_index_white_space(grid_bin_white_space_LUT);
      if (flag_bisection_complete) {
        box1.cut_direction_x = true;
        box2.cut_direction_x = true;
        box1.ur_index = box.cut_ur_index;
        box2.ll_index = box.cut_ll_index;
      }
    }
  }
  box1.update_cell_area_white_space(grid_bin_matrix);
  box2.update_cell_area_white_space(grid_bin_matrix);
  //std::cout << box1.ll_index << box1.ur_index << "\n";
  //std::cout << box2.ll_index << box2.ur_index << "\n";
  //box1.update_all_terminal(grid_bin_matrix);
  //box2.update_all_terminal(grid_bin_matrix);
  // if the white space in one bin is dominating the other, ignore the smaller one
  dominating_box_flag = 0;
  if (box1.total_white_space/(float)box.total_white_space <= 0.01) {
    dominating_box_flag = 1;
  }
  if (box2.total_white_space/(float)box.total_white_space <= 0.01) {
    dominating_box_flag = 2;
  }

  box1.update_boundaries(grid_bin_matrix);
  box2.update_boundaries(grid_bin_matrix);

  if (dominating_box_flag == 0) {
    //std::cout << "cell list size: " << box.cell_list.size() << "\n";
    //box.update_cell_area(Nodelist);
    //std::cout << "total_cell_area: " << box.total_cell_area << "\n";
    box.update_cut_point_cell_list_low_high(Nodelist, box1.total_white_space, box2.total_white_space);
    box1.cell_list = box.cell_list_low;
    box2.cell_list = box.cell_list_high;
    box1.ll_point = box.ll_point;
    box2.ur_point = box.ur_point;
    box1.ur_point = box.cut_ur_point;
    box2.ll_point = box.cut_ll_point;
    box1.total_cell_area = box.total_cell_area_low;
    box2.total_cell_area = box.total_cell_area_high;

    if (box1.ll_index == box1.ur_index) {
      box1.update_terminal_list(grid_bin_matrix);
      box1.update_ob_boundaries(Nodelist);
    }
    if (box2.ll_index == box2.ur_index) {
      box2.update_terminal_list(grid_bin_matrix);
      box2.update_ob_boundaries(Nodelist);
    }

    /*if ((box1.left < LEFT) || (box1.bottom < BOTTOM)) {
      std::cout << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      std::cout << box1.left << " " << box1.bottom << "\n";
    }
    if ((box2.left < LEFT) || (box2.bottom < BOTTOM)) {
      std::cout << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      std::cout << box2.left << " " << box2.bottom << "\n";
    }*/

    queue_box_bin.push(box1);
    queue_box_bin.push(box2);
    //box1.write_box_boundary("first_bounding_box.txt", GRID_BIN_WIDTH, GRID_BIN_HEIGHT, LEFT, BOTTOM);
    //box2.write_box_boundary("first_bounding_box.txt", GRID_BIN_WIDTH, GRID_BIN_HEIGHT, LEFT, BOTTOM);
    //box1.write_cell_region("first_cell_bounding_box.txt");
    //box2.write_cell_region("first_cell_bounding_box.txt");
  } else if (dominating_box_flag == 1) {
    box2.ll_point = box.ll_point;
    box2.ur_point = box.ur_point;
    box2.cell_list = box.cell_list;
    box2.total_cell_area = box.total_cell_area;
    if (box2.ll_index == box2.ur_index) {
      box2.update_terminal_list(grid_bin_matrix);
      box2.update_ob_boundaries(Nodelist);
    }

    /*if ((box2.left < LEFT) || (box2.bottom < BOTTOM)) {
      std::cout << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      std::cout << box2.left << " " << box2.bottom << "\n";
    }*/

    queue_box_bin.push(box2);
    //box2.write_box_boundary("first_bounding_box.txt", GRID_BIN_WIDTH, GRID_BIN_HEIGHT, LEFT, BOTTOM);
    //box2.write_cell_region("first_cell_bounding_box.txt");
  } else {
    box1.ll_point = box.ll_point;
    box1.ur_point = box.ur_point;
    box1.cell_list = box.cell_list;
    box1.total_cell_area = box.total_cell_area;
    if (box1.ll_index == box1.ur_index) {
      box1.update_terminal_list(grid_bin_matrix);
      box1.update_ob_boundaries(Nodelist);
    }

    /*if ((box1.left < LEFT) || (box1.bottom < BOTTOM)) {
      std::cout << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      std::cout << box1.left << " " << box1.bottom << "\n";
    }*/

    queue_box_bin.push(box1);
    //box1.write_box_boundary("first_bounding_box.txt", GRID_BIN_WIDTH, GRID_BIN_HEIGHT, LEFT, BOTTOM);
    //box1.write_cell_region("first_cell_bounding_box.txt");
  }
}

void circuit_t::grid_bin_box_split(box_bin &box) {
  box_bin box1, box2;
  box1.left = box.left;
  box1.bottom = box.bottom;
  box2.right = box.right;
  box2.top = box.top;
  box1.ll_index = box.ll_index;
  box1.ur_index = box.ur_index;
  box2.ll_index = box.ll_index;
  box2.ur_index = box.ur_index;
  /* split along the direction with more boundary lines */
  if (box.horizontal_obstacle_boundaries.size() > box.vertical_obstacle_boundaries.size()) {
    box.cut_direction_x = true;
    box1.right = box.right;
    box1.top = box.horizontal_obstacle_boundaries[0];
    box2.left = box.left;
    box2.bottom = box.horizontal_obstacle_boundaries[0];
    box1.update_terminal_list_white_space(Nodelist, box.terminal_list);
    box2.update_terminal_list_white_space(Nodelist, box.terminal_list);

    if (box1.total_white_space/(float)box.total_white_space <= 0.01) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.cell_list = box.cell_list;
      box2.total_cell_area = box.total_cell_area;
      box2.update_ob_boundaries(Nodelist);
      queue_box_bin.push(box2);
    } else if (box2.total_white_space/(float)box.total_white_space <= 0.01) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.cell_list = box.cell_list;
      box1.total_cell_area = box.total_cell_area;
      box1.update_ob_boundaries(Nodelist);
      queue_box_bin.push(box1);
    } else {
      box.update_cut_point_cell_list_low_high(Nodelist, box1.total_white_space, box2.total_white_space);
      box1.cell_list = box.cell_list_low;
      box2.cell_list = box.cell_list_high;
      box1.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box1.ur_point = box.cut_ur_point;
      box2.ll_point = box.cut_ll_point;
      box1.total_cell_area = box.total_cell_area_low;
      box2.total_cell_area = box.total_cell_area_high;
      box1.update_ob_boundaries(Nodelist);
      box2.update_ob_boundaries(Nodelist);
      queue_box_bin.push(box1);
      queue_box_bin.push(box2);
    }
  } else {
    box.cut_direction_x = false;
    box1.right = box.vertical_obstacle_boundaries[0];
    box1.top = box.top;
    box2.left = box.vertical_obstacle_boundaries[0];
    box2.bottom = box.bottom;
    box1.update_terminal_list_white_space(Nodelist, box.terminal_list);
    box2.update_terminal_list_white_space(Nodelist, box.terminal_list);

    if (box1.total_white_space/(float)box.total_white_space <= 0.01) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.cell_list = box.cell_list;
      box2.total_cell_area = box.total_cell_area;
      box2.update_ob_boundaries(Nodelist);
      queue_box_bin.push(box2);
    } else if (box2.total_white_space/(float)box.total_white_space <= 0.01) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.cell_list = box.cell_list;
      box1.total_cell_area = box.total_cell_area;
      box1.update_ob_boundaries(Nodelist);
      queue_box_bin.push(box1);
    } else {
      box.update_cut_point_cell_list_low_high(Nodelist, box1.total_white_space, box2.total_white_space);
      box1.cell_list = box.cell_list_low;
      box2.cell_list = box.cell_list_high;
      box1.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box1.ur_point = box.cut_ur_point;
      box2.ll_point = box.cut_ll_point;
      box1.total_cell_area = box.total_cell_area_low;
      box2.total_cell_area = box.total_cell_area_high;
      box1.update_ob_boundaries(Nodelist);
      box2.update_ob_boundaries(Nodelist);
      queue_box_bin.push(box1);
      queue_box_bin.push(box2);
    }
  }
}

void circuit_t::cell_placement_in_box(box_bin &box) {
  /* this is the simplest version, just linearly move cells in the cell_box to the grid box
   * non-linearity is not considered yet*/
  float cell_box_left, cell_box_bottom;
  float cell_box_width, cell_box_height;
  cell_box_left = box.ll_point.x;
  cell_box_bottom = box.ll_point.y;
  cell_box_width = box.ur_point.x - cell_box_left;
  cell_box_height = box.ur_point.y - cell_box_bottom;
  node_t *cell;
  for (auto &&cell_id: box.cell_list) {
    cell = &Nodelist[cell_id];
    cell->x0 = (cell->x0 - cell_box_left)/cell_box_width * (box.right - box.left) + box.left;
    cell->y0 = (cell->y0 - cell_box_bottom)/cell_box_height * (box.top - box.bottom) + box.bottom;
    /*if ((box.left < LEFT) || (box.bottom < BOTTOM)) {
      std::cout << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      std::cout << box.left << " " << box.bottom << "\n";
    }*/
  }
}

float circuit_t::cell_overlap(node_t *node1, node_t *node2) {
  bool not_overlap;
  not_overlap = ((node1->llx() >= node2->urx())||(node1->lly() >= node2->ury())) || ((node2->llx() >= node1->urx())||(node2->lly() >= node1->ury()));
  if (not_overlap) {
    return 0;
  } else {
    float node1_llx, node1_lly, node1_urx, node1_ury;
    float node2_llx, node2_lly, node2_urx, node2_ury;
    float max_llx, max_lly, min_urx, min_ury;
    float overlap_x, overlap_y, overlap_area;

    node1_llx = node1->llx();
    node1_lly = node1->lly();
    node1_urx = node1->urx();
    node1_ury = node1->ury();
    node2_llx = node2->llx();
    node2_lly = node2->lly();
    node2_urx = node2->urx();
    node2_ury = node2->ury();

    max_llx = std::max(node1_llx, node2_llx);
    max_lly = std::max(node1_lly, node2_lly);
    min_urx = std::min(node1_urx, node2_urx);
    min_ury = std::min(node1_ury, node2_ury);

    overlap_x = min_urx - max_llx;
    overlap_y = min_ury - max_lly;
    overlap_area = overlap_x * overlap_y;
    return overlap_area;
  }
}

void circuit_t::cell_placement_in_box_molecular_dynamics(box_bin &box) {
  /* this is the simplest version, just linearly move cells in the cell_box to the grid box
   * non-linearity is not considered yet*/
  float cell_box_left, cell_box_bottom;
  float cell_box_width, cell_box_height;
  cell_box_left = box.ll_point.x;
  cell_box_bottom = box.ll_point.y;
  cell_box_width = box.ur_point.x - cell_box_left;
  cell_box_height = box.ur_point.y - cell_box_bottom;
  node_t *cell;
  for (auto &&cell_id: box.cell_list) {
    cell = &Nodelist[cell_id];
    cell->x0 = (cell->x0 - cell_box_left)/cell_box_width * (box.right - box.left) + box.left;
    cell->y0 = (cell->y0 - cell_box_bottom)/cell_box_height * (box.top - box.bottom) + box.bottom;
  }

  std::vector< float > vx, vy;
  for (size_t i=0; i<box.cell_list.size(); i++) {
    vx.push_back(0);
    vy.push_back(0);
  }

  float overlap_ij, rij;
  float tmp_vx, tmp_vy;
  node_t *node_i, *node_j;
  for (int t=0; t<60; t++) {
    // 0. initialize velocity
    for (size_t i=0; i<box.cell_list.size(); i++) {
      vx[i] = 0;
      vy[i] = 0;
    }

    // 1. update velocity based on overlap with other nodes and boundaries
    for (size_t i=0; i<box.cell_list.size(); i++) {
      node_i = &Nodelist[box.cell_list[i]];
      for (size_t j=i+1; j<box.cell_list.size(); j++) {
        node_j = &Nodelist[box.cell_list[j]];
        overlap_ij = cell_overlap(node_i, node_j);
        rij = sqrt(pow(node_i->x0 - node_j->x0, 2) + pow(node_i->y0 - node_j->y0, 2)) + 1;
        tmp_vx = overlap_ij*(node_i->x0 - node_j->x0)/rij * 40;
        tmp_vy = overlap_ij*(node_i->y0 - node_j->y0)/rij * 40;
        vx[i] += tmp_vx/node_i->area();
        vy[i] += tmp_vy/node_i->area();

        vx[j] -= tmp_vx/node_j->area();
        vy[j] -= tmp_vy/node_j->area();
      }

      /* this part need modifications, the overlap between boundaries and cells, some constant should be changed to constant related to ave_height or ave_width */
      if (node_i->llx() < box.left) {
        vx[i]++;
      }
      if (node_i->urx() > box.right) {
        vx[i]--;
      }
      if (node_i->lly() < box.bottom) {
        vy[i]++;
      }
      if (node_i->ury() > box.top) {
        vy[i]--;
      }

    }

    // 2. update location
    for (size_t i=0; i<box.cell_list.size(); i++) {
      node_i = &Nodelist[box.cell_list[i]];
      node_i->x0 += vx[i];
      node_i->y0 += vy[i];
      /*if (node_i->llx() < box.left) {
        node_i->x0 = box.left + node_i->width()/2.0;
      }
      if (node_i->urx() > box.right) {
        node_i->x0 = box.right - node_i->width()/2.0;
      }
      if (node_i->lly() < box.bottom) {
        node_i->y0 = box.bottom + node_i->height()/2.0;
      }
      if (node_i->ury() > box.top) {
        node_i->y0 = box.top - node_i->height()/2.0;
      }*/
    }
  }
}

void circuit_t::cell_placement_in_box_bisection(box_bin &box) {
  /* keep bisect a grid bin until the leaf bin has less than say 2 nodes? */
  size_t max_cell_num_in_box = 1;
  box.cut_direction_x = true;
  std::queue< box_bin > box_Q;
  box_Q.push(box);
  while (!box_Q.empty()) {
    //std::cout << " Q.size: " << box_Q.size() << "\n";
    box_bin &front_box = box_Q.front();
    if (front_box.cell_list.size() > max_cell_num_in_box) {
      /*
      std::cout << front_box.cell_list.size() << "\n";
      std::cout << front_box.left << " " << front_box.right << " " << front_box.bottom << " " << front_box.top << "\n";
      std::cout << front_box.total_cell_area << "\n";
      node_t *node;
      for (auto &&cell_id: front_box.cell_list) {
        node = &Nodelist[cell_id];
        std::cout << node->x0 << ", " << node->y0 << "\n";
      }
      */

      // split box and push it to box_Q
      box_bin box1, box2;
      box1.ur_index = front_box.ur_index;
      box2.ll_index = front_box.ll_index;
      box1.bottom = front_box.bottom;
      box1.left = front_box.left;
      box2.top = front_box.top;
      box2.right = front_box.right;

      if (front_box.top - front_box.bottom <= std_cell_height) {
        front_box.cut_direction_x = false;
      } else {
        front_box.cut_direction_x = true;
      }

      int cut_line_w = 0; // cut-line for White space
      front_box.update_cut_point_cell_list_low_high_leaf(Nodelist, cut_line_w, std_cell_height);
      if (front_box.cut_direction_x) {
        box1.top = cut_line_w;
        box1.right = front_box.right;

        box2.bottom = cut_line_w;
        box2.left = front_box.left;
      } else {
        box1.top = front_box.top;
        box1.right = cut_line_w;

        box2.bottom = front_box.bottom;
        box2.left = cut_line_w;
      }
      box1.cell_list = front_box.cell_list_low;
      box2.cell_list = front_box.cell_list_high;
      box1.ll_point = front_box.ll_point;
      box2.ur_point = front_box.ur_point;
      box1.ur_point = front_box.cut_ur_point;
      box2.ll_point = front_box.cut_ll_point;
      box1.total_cell_area = front_box.total_cell_area_low;
      box2.total_cell_area = front_box.total_cell_area_high;

      if (!box1.cell_list.empty()) {
        box_Q.push(box1);
        if ((box1.top - box1.bottom) % std_cell_height != 0) {
          std::cout << "Error in grib bin split\n";
          std::cout << box1.left << " " << box1.right << " " << box1.bottom << " " << box1.top << "\n";
        }
      }
      if (!box2.cell_list.empty()) {
        box_Q.push(box2);
        if ((box2.top - box2.bottom) % std_cell_height != 0) {
          std::cout << "Error in grib bin split\n";
          std::cout << box2.left << " " << box2.right << " " << box2.bottom << " " << box2.top << "\n";
        }
      }
    } else {
      // shift cells to the center of the final box
      if (max_cell_num_in_box == 1) {
        node_t *cell;
        for (auto &&cell_id: front_box.cell_list) {
          cell = &Nodelist[cell_id];
          cell->x0 = (front_box.left + front_box.right)/2.0;
          cell->y0 = (front_box.bottom + front_box.top)/2.0;
        }
      } else {
        cell_placement_in_box(front_box);
      }
    }
    box_Q.pop();
  }

}

bool circuit_t::recursive_bisection_cell_spreading() {
  /* keep splitting the biggest box to many small boxes, and keep update the shape of each box and cells should be assigned to the box */
  int t = 0;
  while(!queue_box_bin.empty()) {
  //for (size_t t=0; t<11000; t++) {
    std::cout << t++ << "\n";
    std::cout << " Q.size: " << queue_box_bin.size() << "\n";
    if (queue_box_bin.empty()) break;
    box_bin &box = queue_box_bin.front();
    /* when the box is a grid bin box or a smaller box with no terminals inside, start moving cells to the box */
    /* if there is terminals inside a box, keep splitting it */
    if (box.ll_index == box.ur_index) {
      if (box.has_terminal()) {
        grid_bin_box_split(box);
        queue_box_bin.pop();
        continue;
      }
      /* if no terminals in side a box, do cell placement inside the box */
      //cell_placement_in_box(box);
      //cell_placement_in_box_molecular_dynamics(box);
      cell_placement_in_box_bisection(box);
      box.write_cell_region("first_cell_bounding_box.txt");
      box.write_box_boundary("first_bounding_box.txt");
      box.write_cell_in_box("nodes.txt", Nodelist);
      queue_box_bin.pop();
    } else {
      box_split(box);
      queue_box_bin.pop();
    }
  }
  return true;
}

void circuit_t::look_ahead_legal() {

  std::ofstream ost("nodes.txt");
  if (ost.is_open()==0) {
    std::cout << "Cannot open nodes.txt\n";
  }
  ost.close();
  ost.open("first_cell_bounding_box.txt");
  if (ost.is_open()==0) {
    std::cout << "Cannot open nodes.txt\n";
  }
  ost.close();
  ost.open("first_bounding_box.txt");
  if (ost.is_open()==0) {
    std::cout << "Cannot open nodes.txt\n";
  }
  ost.close();


  do {
  //for (int t=0; t<1; t++) {
    update_grid_bins_state();

    /* print out grid_bins (terminal.txt) and nodes (nodes.txt) in order to verify the above initialization is correct */
    //write_node_terminal();
    //write_overfill_grid_bins();
    //write_not_overfill_grid_bins();

    cluster_sort_grid_bins();
    //std::cout << "Number of clusters: " << cluster_list.size() << "\n";

    //write_first_bin_cluster();
    //write_all_bin_cluster();

    find_box_first_cluster();
    //std::cout << "Total cell list size before bisection: " << queue_box_bin.front().cell_list.size() << "\n";
    //queue_box_bin.front().write_cell_in_box("nodes.txt", Nodelist);

    //write_first_box();
    //write_first_box_cell_bounding();

    // for debug purpose, use with member function write_cell_in_box, to create a empty nodes.txt

    recursive_bisection_cell_spreading();
  } while (!cluster_list.empty());
}

void circuit_t::copy_xy_to_anchor () {
  for (auto &&node: Nodelist){
    node.cp_x_2_anchor();
    node.cp_y_2_anchor();
  }
}

void circuit_t::swap_xy_anchor() {
  node_t *node;
  for (size_t cell_id=0; cell_id < CELL_NUM; cell_id++){
    node = &Nodelist[cell_id];
    node->swap_x_anchor();
    node->swap_y_anchor();
  }
}
