/*******************************************************************************
 *
 * Copyright (c) 2022 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/

#include "rough_legalizer.h"

#include "dali/common/elapsed_time.h"
#include "dali/common/logging.h"

namespace dali {

RoughLegalizer::RoughLegalizer(Circuit *ckt_ptr) {
  DaliExpects(ckt_ptr != nullptr, "Circuit is a nullptr?");
  ckt_ptr_ = ckt_ptr;
}

void RoughLegalizer::SetShouldSaveIntermediateResult(bool should_save_intermediate_result) {
  should_save_intermediate_result_ = should_save_intermediate_result;
}

/****
 * @brief determine the grid bin height and width
 * grid_bin_height and grid_bin_width is determined by the following formula:
 *    grid_bin_height = sqrt(number_of_cell_in_bin_ * average_area / placement_density)
 * the number of bins in the y-direction is given by:
 *    grid_cnt_y = (Top() - Bottom())/grid_bin_height
 *    grid_cnt_x = (Right() - Left())/grid_bin_width
 * And initialize the space of grid_bin_mesh
 */
void LookAheadLegalizer::InitializeGridBinSize() {
  double grid_bin_area =
      number_of_cell_in_bin_ * ckt_ptr_->AveMovBlkArea() / placement_density_;
  grid_bin_height = static_cast<int32_t>(std::round(std::sqrt(grid_bin_area)));
  grid_bin_width = grid_bin_height;
  grid_cnt_x = std::ceil(double(ckt_ptr_->RegionWidth()) / grid_bin_width);
  grid_cnt_y = std::ceil(double(ckt_ptr_->RegionHeight()) / grid_bin_height);
  BOOST_LOG_TRIVIAL(debug)
    << "  Global placement bin width, height: "
    << grid_bin_width << "  " << grid_bin_height << "\n";

  std::vector<GridBin> temp_grid_bin_column(grid_cnt_y);
  grid_bin_mesh.resize(grid_cnt_x, temp_grid_bin_column);
}

/****
 * @brief set basic attributes for each grid bin.
 * we need to initialize many attributes in every single grid bin, including index,
 * boundaries, area, and potential available white space. The adjacent bin list
 * is cached for the convenience of overfilled bin clustering.
 */
void LookAheadLegalizer::UpdateAttributesForAllGridBins() {
  for (int32_t i = 0; i < grid_cnt_x; i++) {
    for (int32_t j = 0; j < grid_cnt_y; j++) {
      grid_bin_mesh[i][j].index = {i, j};
      grid_bin_mesh[i][j].bottom = ckt_ptr_->RegionLLY() + j * grid_bin_height;
      grid_bin_mesh[i][j].top =
          ckt_ptr_->RegionLLY() + (j + 1) * grid_bin_height;
      grid_bin_mesh[i][j].left = ckt_ptr_->RegionLLX() + i * grid_bin_width;
      grid_bin_mesh[i][j].right =
          ckt_ptr_->RegionLLX() + (i + 1) * grid_bin_width;
      grid_bin_mesh[i][j].white_space = grid_bin_mesh[i][j].Area();
      // at the very beginning, assuming the white space is the same as area
      grid_bin_mesh[i][j].create_adjacent_bin_list(grid_cnt_x, grid_cnt_y);
    }
  }

  // make sure the top placement boundary is the same as the top of the topmost bins
  for (int32_t i = 0; i < grid_cnt_x; ++i) {
    grid_bin_mesh[i][grid_cnt_y - 1].top = ckt_ptr_->RegionURY();
    grid_bin_mesh[i][grid_cnt_y - 1].white_space =
        grid_bin_mesh[i][grid_cnt_y - 1].Area();
  }
  // make sure the right placement boundary is the same as the right of the rightmost bins
  for (int32_t i = 0; i < grid_cnt_y; ++i) {
    grid_bin_mesh[grid_cnt_x - 1][i].right = ckt_ptr_->RegionURX();
    grid_bin_mesh[grid_cnt_x - 1][i].white_space =
        grid_bin_mesh[grid_cnt_x - 1][i].Area();
  }
}

/****
 * @brief find fixed blocks in each grid bin
 * For each fixed block, we need to store its index in grid bins it overlaps with.
 * This can help us to compute available white space in each grid bin.
 */
void LookAheadLegalizer::UpdateFixedBlocksInGridBins() {
  for (auto &&blk : ckt_ptr_->Blocks()) {
    /* find the left, right, bottom, top index of the grid */
    if (blk.IsMovable()) continue;
    bool fixed_blk_out_of_region = int32_t(blk.LLX()) >= ckt_ptr_->RegionURX()
        || int32_t(blk.URX()) <= ckt_ptr_->RegionLLX()
        || int32_t(blk.LLY()) >= ckt_ptr_->RegionURY()
        || int32_t(blk.URY()) <= ckt_ptr_->RegionLLY();
    // TODO: test and clean up this part of code using an adaptec benchmark
    if (fixed_blk_out_of_region) continue;
    int32_t left_index = std::floor(
        (blk.LLX() - ckt_ptr_->RegionLLX()) / grid_bin_width);
    int32_t right_index = std::floor(
        (blk.URX() - ckt_ptr_->RegionLLX()) / grid_bin_width);
    int32_t bottom_index = std::floor(
        (blk.LLY() - ckt_ptr_->RegionLLY()) / grid_bin_height);
    int32_t top_index = std::floor(
        (blk.URY() - ckt_ptr_->RegionLLY()) / grid_bin_height);
    /* the grid boundaries might be the placement region boundaries
     * if a block touches the rightmost and topmost boundaries,
     * the index need to be fixed to make sure no memory access out of scope */
    if (left_index < 0) left_index = 0;
    if (right_index >= grid_cnt_x) right_index = grid_cnt_x - 1;
    if (bottom_index < 0) bottom_index = 0;
    if (top_index >= grid_cnt_y) top_index = grid_cnt_y - 1;

    /* for each terminal, we will check which grid is inside it, and directly
     * set the all_terminal attribute to true for that grid some small
     * terminals might occupy the same grid, we need to deduct the overlap
     * area from the white space of that grid bin when the final white space
     * is 0, we know this grid bin is occupied by several terminals*/
    for (int32_t j = left_index; j <= right_index; ++j) {
      for (int32_t k = bottom_index; k <= top_index; ++k) {
        /* the following case might happen:
         * the top/right of a fixed block overlap with the bottom/left of
         * a grid box. if this case happens, we need to ignore this fixed
         * block for this grid box. */
        bool blk_out_of_bin =
            int32_t(blk.LLX() >= grid_bin_mesh[j][k].right) ||
                int32_t(blk.URX() <= grid_bin_mesh[j][k].left) ||
                int32_t(blk.LLY() >= grid_bin_mesh[j][k].top) ||
                int32_t(blk.URY() <= grid_bin_mesh[j][k].bottom);
        if (blk_out_of_bin) continue;
        grid_bin_mesh[j][k].fixed_blocks.push_back(&blk);
      }
    }
  }
}

void LookAheadLegalizer::UpdateWhiteSpaceInGridBin(GridBin &grid_bin) {
  RectI bin_rect(
      grid_bin.LLX(), grid_bin.LLY(), grid_bin.URX(), grid_bin.URY()
  );

  std::vector<RectI> rects;
  for (auto &fixed_blk_ptr : grid_bin.fixed_blocks) {
    auto &fixed_blk = *fixed_blk_ptr;
    RectI fixed_blk_rect(
        static_cast<int32_t>(std::round(fixed_blk.LLX())),
        static_cast<int32_t>(std::round(fixed_blk.LLY())),
        static_cast<int32_t>(std::round(fixed_blk.URX())),
        static_cast<int32_t>(std::round(fixed_blk.URY()))
    );
    if (bin_rect.IsOverlap(fixed_blk_rect)) {
      rects.push_back(bin_rect.GetOverlapRect(fixed_blk_rect));
    }
  }

  unsigned long long used_area = GetCoverArea(rects);
  DaliExpects(
      grid_bin.white_space >= used_area,
      "Fixed blocks takes more space than available space? "
          << grid_bin.white_space << " " << used_area
  );

  grid_bin.white_space -= used_area;
  if (grid_bin.white_space == 0) {
    grid_bin.all_terminal = true;
  }
}

/****
 * This function initialize the grid bin matrix, each bin has an area which
 * can accommodate around number_of_cell_in_bin_ # of cells
 * ****/
void LookAheadLegalizer::InitGridBins() {
  InitializeGridBinSize();
  UpdateAttributesForAllGridBins();
  UpdateFixedBlocksInGridBins();

  // update white spaces in grid bins
  for (auto &grid_bin_column : grid_bin_mesh) {
    for (auto &grid_bin : grid_bin_column) {
      UpdateWhiteSpaceInGridBin(grid_bin);
    }
  }
}

/****
* this is a member function to initialize white space look-up table
* this table is a matrix, one way to calculate the white space in a region is to add all white space of every single grid bin in this region
* an easier way is to define an accumulate function and store it as a look-up table
* when we want to find the white space in a region, the value can be easily extracted from the look-up table
* ****/
void LookAheadLegalizer::InitWhiteSpaceLUT() {
  // this for loop is created to initialize the size of the loop-up table
  std::vector<unsigned long long> tmp_vector(grid_cnt_y);
  grid_bin_white_space_LUT.resize(grid_cnt_x, tmp_vector);

  // this for loop is used for computing elements in the look-up table
  // there are four cases, element at (0,0), elements on the left edge, elements on the right edge, otherwise
  for (int32_t kx = 0; kx < grid_cnt_x; ++kx) {
    for (int32_t ky = 0; ky < grid_cnt_y; ++ky) {
      grid_bin_white_space_LUT[kx][ky] = 0;
      if (kx == 0) {
        if (ky == 0) {
          grid_bin_white_space_LUT[kx][ky] = grid_bin_mesh[0][0].white_space;
        } else {
          grid_bin_white_space_LUT[kx][ky] =
              grid_bin_white_space_LUT[kx][ky - 1]
                  + grid_bin_mesh[kx][ky].white_space;
        }
      } else {
        if (ky == 0) {
          grid_bin_white_space_LUT[kx][ky] =
              grid_bin_white_space_LUT[kx - 1][ky]
                  + grid_bin_mesh[kx][ky].white_space;
        } else {
          grid_bin_white_space_LUT[kx][ky] =
              grid_bin_white_space_LUT[kx - 1][ky]
                  + grid_bin_white_space_LUT[kx][ky - 1]
                  + grid_bin_mesh[kx][ky].white_space
                  - grid_bin_white_space_LUT[kx - 1][ky - 1];
        }
      }
    }
  }
}

void LookAheadLegalizer::Initialize(double placement_density) {
  placement_density_ = placement_density;

  upper_bound_hpwl_x_.clear();
  upper_bound_hpwl_y_.clear();
  upper_bound_hpwl_.clear();
  InitGridBins();
  InitWhiteSpaceLUT();
}

void LookAheadLegalizer::ClearGridBinFlag() {
  for (auto &bin_column : grid_bin_mesh) {
    for (auto &bin : bin_column) bin.global_placed = false;
  }
}

/****
 * this is a member function to update grid bin status, because the cell_list,
 * cell_area and over_fill state can be changed, so we need to update them when necessary
 * ****/
void LookAheadLegalizer::UpdateGridBinState() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  // clean the old data
  for (auto &grid_bin_column : grid_bin_mesh) {
    for (auto &grid_bin : grid_bin_column) {
      grid_bin.cell_list.clear();
      grid_bin.cell_area = 0;
      grid_bin.over_fill = false;
    }
  }

  // for each cell, find the index of the grid bin it should be in.
  // note that in extreme cases, the index might be smaller than 0 or larger
  // than the maximum allowed index, because the cell is on the boundaries,
  // so we need to make some modifications for these extreme cases.
  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  int32_t sz = static_cast<int32_t>(blocks.size());
  int32_t x_index = 0;
  int32_t y_index = 0;

  for (int32_t i = 0; i < sz; i++) {
    if (blocks[i].IsFixed()) continue;
    x_index = (int32_t) std::floor(
        (blocks[i].X() - ckt_ptr_->RegionLLX()) / grid_bin_width);
    y_index =
        (int32_t) std::floor(
            (blocks[i].Y() - ckt_ptr_->RegionLLY()) / grid_bin_height);
    if (x_index < 0) x_index = 0;
    if (x_index > grid_cnt_x - 1) x_index = grid_cnt_x - 1;
    if (y_index < 0) y_index = 0;
    if (y_index > grid_cnt_y - 1) y_index = grid_cnt_y - 1;
    grid_bin_mesh[x_index][y_index].cell_list.push_back(&(blocks[i]));
    grid_bin_mesh[x_index][y_index].cell_area += blocks[i].Area();
  }

  /**** below is the criterion to decide whether a grid bin is over_filled or not
   * 1. if this bin if fully occupied by fixed blocks, but its cell_list is
   *    non-empty, which means there is some cells overlap with this grid bin,
   *    we say it is over_fill
   * 2. if not fully occupied by fixed blocks, but filling_rate is larger than
   *    the TARGET_FILLING_RATE, then set is to over_fill
   * 3. if this bin is not overfilled, but cells in this bin overlaps with fixed
   *    blocks in this bin, we also mark it as over_fill
   * ****/
  //TODO: the third criterion might be changed in the next
  bool over_fill = false;
  for (auto &grid_bin_column : grid_bin_mesh) {
    for (auto &grid_bin : grid_bin_column) {
      if (grid_bin.global_placed) {
        grid_bin.over_fill = false;
        continue;
      }
      if (grid_bin.IsAllFixedBlk()) {
        if (!grid_bin.cell_list.empty()) {
          grid_bin.over_fill = true;
        }
      } else {
        grid_bin.filling_rate =
            double(grid_bin.cell_area) / double(grid_bin.white_space);
        if (grid_bin.filling_rate > placement_density_) {
          grid_bin.over_fill = true;
        }
      }
      if (!grid_bin.OverFill()) {
        for (auto &blk_ptr : grid_bin.cell_list) {
          for (auto &fixed_blk_ptr : grid_bin.fixed_blocks) {
            over_fill = blk_ptr->IsOverlap(*fixed_blk_ptr);
            if (over_fill) {
              grid_bin.over_fill = true;
              break;
            }
          }
          if (over_fill) break;
          // two breaks have to be used to break two loops
        }
      }
    }
  }
  elapsed_time.RecordEndTime();
  update_grid_bin_state_time_ += elapsed_time.GetWallTime();
}

void LookAheadLegalizer::UpdateClusterArea(GridBinCluster &cluster) {
  cluster.total_cell_area = 0;
  cluster.total_white_space = 0;
  for (auto &index : cluster.bin_set) {
    cluster.total_cell_area +=
        grid_bin_mesh[index.x][index.y].cell_area;
    cluster.total_white_space +=
        grid_bin_mesh[index.x][index.y].white_space;
  }
}

void LookAheadLegalizer::UpdateClusterList() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();
  cluster_set.clear();

  int32_t m = (int32_t) grid_bin_mesh.size(); // number of rows
  int32_t n = (int32_t) grid_bin_mesh[0].size(); // number of columns
  for (int32_t i = 0; i < m; ++i) {
    for (int32_t j = 0; j < n; ++j)
      grid_bin_mesh[i][j].cluster_visited = false;
  }
  int32_t cnt = 0;
  for (int32_t i = 0; i < m; ++i) {
    for (int32_t j = 0; j < n; ++j) {
      if (grid_bin_mesh[i][j].cluster_visited
          || !grid_bin_mesh[i][j].over_fill)
        continue;
      GridBinIndex b(i, j);
      GridBinCluster H;
      H.bin_set.insert(b);
      grid_bin_mesh[i][j].cluster_visited = true;
      cnt = 0;
      std::queue<GridBinIndex> Q;
      Q.push(b);
      while (!Q.empty()) {
        b = Q.front();
        Q.pop();
        for (auto
              &index : grid_bin_mesh[b.x][b.y].adjacent_bin_index) {
          GridBin &bin = grid_bin_mesh[index.x][index.y];
          if (!bin.cluster_visited && bin.over_fill) {
            if (cnt > cluster_upper_size) {
              UpdateClusterArea(H);
              cluster_set.insert(H);
              break;
            }
            bin.cluster_visited = true;
            H.bin_set.insert(index);
            ++cnt;
            Q.push(index);
          }
        }
      }
      UpdateClusterArea(H);
      cluster_set.insert(H);
    }
  }
  elapsed_time.RecordEndTime();
  update_cluster_list_time_ += elapsed_time.GetWallTime();
}

void LookAheadLegalizer::UpdateLargestCluster() {
  if (cluster_set.empty()) return;

  for (auto it = cluster_set.begin(); it != cluster_set.end();) {
    bool is_contact = true;

    // if there is no grid bin has been roughly legalized, then this cluster is the largest one for sure
    for (auto &index : it->bin_set) {
      if (grid_bin_mesh[index.x][index.y].global_placed) {
        is_contact = false;
      }
    }
    if (is_contact) break;

    // initialize a list to store all indices in this cluster
    // initialize a map to store the visited flag during bfs
    std::vector<GridBinIndex> grid_bin_list;
    grid_bin_list.reserve(it->bin_set.size());
    std::unordered_map<GridBinIndex, bool, GridBinIndexHasher>
        grid_bin_visited;
    for (auto &index : it->bin_set) {
      grid_bin_list.push_back(index);
      grid_bin_visited.insert({index, false});
    }

    std::sort(grid_bin_list.begin(),
              grid_bin_list.end(),
              [](const GridBinIndex &index1, const GridBinIndex &index2) {
                return (index1.x < index2.x)
                    || (index1.x == index2.x && index1.y < index2.y);
              });

    int32_t cnt = 0;
    for (auto &grid_index : grid_bin_list) {
      int32_t i = grid_index.x;
      int32_t j = grid_index.y;
      if (grid_bin_visited[grid_index]) continue; // if this grid bin has been visited continue
      if (grid_bin_mesh[i][j].global_placed) continue; // if this grid bin has been roughly legalized
      GridBinIndex b(i, j);
      GridBinCluster H;
      H.bin_set.insert(b);
      grid_bin_visited[grid_index] = true;
      cnt = 0;
      std::queue<GridBinIndex> Q;
      Q.push(b);
      while (!Q.empty()) {
        b = Q.front();
        Q.pop();
        for (auto
              &index : grid_bin_mesh[b.x][b.y].adjacent_bin_index) {
          if (grid_bin_visited.find(index)
              == grid_bin_visited.end())
            continue; // this index is not in the cluster
          if (grid_bin_visited[index]) continue; // this index has been visited
          if (grid_bin_mesh[index.x][index.y].global_placed) continue; // if this grid bin has been roughly legalized
          if (cnt > cluster_upper_size) {
            UpdateClusterArea(H);
            cluster_set.insert(H);
            break;
          }
          grid_bin_visited[index] = true;
          H.bin_set.insert(index);
          ++cnt;
          Q.push(index);
        }
      }
      UpdateClusterArea(H);
      cluster_set.insert(H);
    }

    it = cluster_set.erase(it);
  }
}

uint32_t LookAheadLegalizer::LookUpWhiteSpace(GridBinIndex const &ll_index,
                                              GridBinIndex const &ur_index) {
  /****
 * this function is used to return the white space in a region specified by ll_index, and ur_index
 * there are four cases, element at (0,0), elements on the left edge, elements on the right edge, otherwise
 * ****/

  uint32_t total_white_space;
  /*if (ll_index.x == 0) {
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
}*/
  WindowQuadruple window = {ll_index.x, ll_index.y, ur_index.x, ur_index.y};
  total_white_space = LookUpWhiteSpace(window);
  return total_white_space;
}

uint32_t LookAheadLegalizer::LookUpWhiteSpace(WindowQuadruple &window) {
  uint32_t total_white_space;
  if (window.llx == 0) {
    if (window.lly == 0) {
      total_white_space =
          grid_bin_white_space_LUT[window.urx][window.ury];
    } else {
      total_white_space = grid_bin_white_space_LUT[window.urx][window.ury]
          - grid_bin_white_space_LUT[window.urx][window.lly - 1];
    }
  } else {
    if (window.lly == 0) {
      total_white_space = grid_bin_white_space_LUT[window.urx][window.ury]
          - grid_bin_white_space_LUT[window.llx - 1][window.ury];
    } else {
      total_white_space = grid_bin_white_space_LUT[window.urx][window.ury]
          - grid_bin_white_space_LUT[window.urx][window.lly - 1]
          - grid_bin_white_space_LUT[window.llx - 1][window.ury]
          + grid_bin_white_space_LUT[window.llx - 1][window.lly - 1];
    }
  }
  return total_white_space;
}

void LookAheadLegalizer::FindMinimumBoxForLargestCluster() {
  /****
  * this function find the box for the largest cluster,
  * such that the total white space in the box is larger than the total cell area
  * the way to do this is just by expanding the boundaries of the bounding box of the first cluster
  *
  * Part 1
  * find the index of the maximum cluster
  * ****/
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  // clear the queue_box_bin
  while (!queue_box_bin.empty()) queue_box_bin.pop();
  if (cluster_set.empty()) return;

  // Part 1
  BoxBin R;
  R.cut_direction_x = false;

  R.ll_index.x = grid_cnt_x - 1;
  R.ll_index.y = grid_cnt_y - 1;
  R.ur_index.x = 0;
  R.ur_index.y = 0;
  // initialize a box with y cut-direction
  // identify the bounding box of the initial cluster
  auto it = cluster_set.begin();
  for (auto &index : it->bin_set) {
    R.ll_index.x = std::min(R.ll_index.x, index.x);
    R.ur_index.x = std::max(R.ur_index.x, index.x);
    R.ll_index.y = std::min(R.ll_index.y, index.y);
    R.ur_index.y = std::max(R.ur_index.y, index.y);
  }
  while (true) {
    // update cell area, white space, and thus filling rate to determine whether to expand this box or not
    R.total_white_space = LookUpWhiteSpace(R.ll_index, R.ur_index);
    R.UpdateCellAreaWhiteSpaceFillingRate(grid_bin_white_space_LUT,
                                          grid_bin_mesh);
    if (R.filling_rate > placement_density_) {
      R.ExpandBox(grid_cnt_x, grid_cnt_y);
    } else {
      break;
    }
    //BOOST_LOG_TRIVIAL(info)   << R.total_white_space << "  " << R.filling_rate << "  " << FillingRate() << "\n";
  }

  R.total_white_space = LookUpWhiteSpace(R.ll_index, R.ur_index);
  R.UpdateCellAreaWhiteSpaceFillingRate(grid_bin_white_space_LUT,
                                        grid_bin_mesh);
  R.UpdateCellList(grid_bin_mesh);
  R.ll_point.x = grid_bin_mesh[R.ll_index.x][R.ll_index.y].left;
  R.ll_point.y = grid_bin_mesh[R.ll_index.x][R.ll_index.y].bottom;
  R.ur_point.x = grid_bin_mesh[R.ur_index.x][R.ur_index.y].right;
  R.ur_point.y = grid_bin_mesh[R.ur_index.x][R.ur_index.y].top;

  R.left = int32_t(R.ll_point.x);
  R.bottom = int32_t(R.ll_point.y);
  R.right = int32_t(R.ur_point.x);
  R.top = int32_t(R.ur_point.y);

  if (R.ll_index == R.ur_index) {
    R.UpdateFixedBlkList(grid_bin_mesh);
    if (R.IsContainFixedBlk()) {
      R.UpdateObsBoundary();
    }
  }
  queue_box_bin.push(R);
  //BOOST_LOG_TRIVIAL(info)   << "Bounding box total white space: " << queue_box_bin.front().total_white_space << "\n";
  //BOOST_LOG_TRIVIAL(info)   << "Bounding box total cell area: " << queue_box_bin.front().total_cell_area << "\n";

  for (int32_t kx = R.ll_index.x; kx <= R.ur_index.x; ++kx) {
    for (int32_t ky = R.ll_index.y; ky <= R.ur_index.y; ++ky) {
      grid_bin_mesh[kx][ky].global_placed = true;
    }
  }

  elapsed_time.RecordEndTime();
  find_minimum_box_for_largest_cluster_time_ += elapsed_time.GetWallTime();
}

/****
 *
 *
 * @param box: the BoxBin which needs to be further splitted into two sub-boxes
 */
void LookAheadLegalizer::SplitGridBox(BoxBin &box) {
  // 1. create two sub-boxes
  BoxBin box1, box2;
  // the first sub-box should have the same lower left corner as the original box
  box1.left = box.left;
  box1.bottom = box.bottom;
  box1.ll_index = box.ll_index;
  box1.ur_index = box.ur_index;
  // the second sub-box should have the same upper right corner as the original box
  box2.right = box.right;
  box2.top = box.top;
  box2.ll_index = box.ll_index;
  box2.ur_index = box.ur_index;

  // 2. split along the direction with more boundary lines
  if (box.IsMoreHorizontalCutlines()) {
    box.cut_direction_x = true;
    // split the original box along the first horizontal cur line
    box1.right = box.right;
    box1.top = box.horizontal_cutlines[0];
    box2.left = box.left;
    box2.bottom = box.horizontal_cutlines[0];
    box1.UpdateWhiteSpaceAndFixedblocks(box.fixed_blocks);
    box2.UpdateWhiteSpaceAndFixedblocks(box.fixed_blocks);

    if (
        double(box1.total_white_space) / (double) box.total_white_space <= 0.01
        ) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.cell_list = box.cell_list;
      box2.total_cell_area = box.total_cell_area;
      box2.UpdateObsBoundary();
      queue_box_bin.push(box2);
    } else if (
        double(box2.total_white_space) / (double) box.total_white_space <= 0.01
        ) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.cell_list = box.cell_list;
      box1.total_cell_area = box.total_cell_area;
      box1.UpdateObsBoundary();
      queue_box_bin.push(box1);
    } else {
      box.update_cut_point_cell_list_low_high(
          box1.total_white_space,
          box2.total_white_space
      );
      box1.cell_list = box.cell_list_low;
      box2.cell_list = box.cell_list_high;
      box1.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box1.ur_point = box.cut_ur_point;
      box2.ll_point = box.cut_ll_point;
      box1.total_cell_area = box.total_cell_area_low;
      box2.total_cell_area = box.total_cell_area_high;
      box1.UpdateObsBoundary();
      box2.UpdateObsBoundary();
      queue_box_bin.push(box1);
      queue_box_bin.push(box2);
    }
  } else {
    //box.Report();
    box.cut_direction_x = false;
    box1.right = box.vertical_cutlines[0];
    box1.top = box.top;
    box2.left = box.vertical_cutlines[0];
    box2.bottom = box.bottom;
    box1.UpdateWhiteSpaceAndFixedblocks(box.fixed_blocks);
    box2.UpdateWhiteSpaceAndFixedblocks(box.fixed_blocks);

    if (
        double(box1.total_white_space) / (double) box.total_white_space <= 0.01
        ) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.cell_list = box.cell_list;
      box2.total_cell_area = box.total_cell_area;
      box2.UpdateObsBoundary();
      queue_box_bin.push(box2);
    } else if (
        double(box2.total_white_space) / (double) box.total_white_space <= 0.01
        ) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.cell_list = box.cell_list;
      box1.total_cell_area = box.total_cell_area;
      box1.UpdateObsBoundary();
      queue_box_bin.push(box1);
    } else {
      box.update_cut_point_cell_list_low_high(
          box1.total_white_space,
          box2.total_white_space
      );
      box1.cell_list = box.cell_list_low;
      box2.cell_list = box.cell_list_high;
      box1.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box1.ur_point = box.cut_ur_point;
      box2.ll_point = box.cut_ll_point;
      box1.total_cell_area = box.total_cell_area_low;
      box2.total_cell_area = box.total_cell_area_high;
      box1.UpdateObsBoundary();
      box2.UpdateObsBoundary();
      queue_box_bin.push(box1);
      queue_box_bin.push(box2);
    }
  }
}

void LookAheadLegalizer::PlaceBlkInBox(BoxBin &box) {
  /* this is the simplest version, just linearly move cells in the cell_box to the grid box
* non-linearity is not considered yet*/

  /*double cell_box_left, cell_box_bottom;
double cell_box_width, cell_box_height;
cell_box_left = box.ll_point.x;
cell_box_bottom = box.ll_point.y;
cell_box_width = box.ur_point.x - cell_box_left;
cell_box_height = box.ur_point.y - cell_box_bottom;
Block *cell;

for (auto &cell_id: box.cell_list) {
  cell = &block_list[cell_id];
  cell->SetCenterX((cell->X() - cell_box_left)/cell_box_width * (box.right - box.left) + box.left);
  cell->SetCenterY((cell->Y() - cell_box_bottom)/cell_box_height * (box.top - box.bottom) + box.bottom);
}*/

  int32_t sz = static_cast<int32_t>(box.cell_list.size());
  std::vector<std::pair<Block *, double>> index_loc_list_x(sz);
  std::vector<std::pair<Block *, double>> index_loc_list_y(sz);
  GridBin &grid_bin = grid_bin_mesh[box.ll_index.x][box.ll_index.y];
  for (int32_t i = 0; i < sz; ++i) {
    Block *blk_ptr = box.cell_list[i];
    index_loc_list_x[i].first = blk_ptr;
    index_loc_list_x[i].second = blk_ptr->X();
    index_loc_list_y[i].first = blk_ptr;
    index_loc_list_y[i].second = blk_ptr->Y();
    grid_bin.cell_list.push_back(blk_ptr);
    grid_bin.cell_area += blk_ptr->Area();
  }

  std::sort(
      index_loc_list_x.begin(),
      index_loc_list_x.end(),
      [](const std::pair<Block *, double> &p1,
         const std::pair<Block *, double> &p2) {
        return p1.second < p2.second;
      }
  );
  double total_length = 0;
  for (auto &blk_ptr : box.cell_list) {
    total_length += blk_ptr->Width();
  }
  double cur_pos = 0;
  int32_t box_width = box.right - box.left;
  for (auto &pair : index_loc_list_x) {
    Block *blk_ptr = pair.first;
    double center_x = box.left + cur_pos / total_length * box_width;
    blk_ptr->SetCenterX(center_x);
    cur_pos += blk_ptr->Width();
    if (std::isnan(center_x)) {
      std::cout << "x " << total_length << "\n";
      box.Report();
      std::cout << std::endl;
      exit(1);
    }
  }

  std::sort(
      index_loc_list_y.begin(),
      index_loc_list_y.end(),
      [](const std::pair<Block *, double> &p1,
         const std::pair<Block *, double> &p2) {
        return p1.second < p2.second;
      }
  );
  total_length = 0;
  for (auto &blk_ptr : box.cell_list) {
    total_length += blk_ptr->Height();
  }
  cur_pos = 0;
  int32_t box_height = box.top - box.bottom;
  for (auto &pair : index_loc_list_y) {
    Block *blk_ptr = pair.first;
    double center_y = box.bottom + cur_pos / total_length * box_height;
    if (std::isnan(center_y)) {
      std::cout << "y " << total_length << "\n";
      box.Report();
      std::cout << std::endl;
      exit(1);
    }
    blk_ptr->SetCenterY(center_y);
    cur_pos += blk_ptr->Height();
  }
}

void LookAheadLegalizer::SplitBox(BoxBin &box) {
  bool flag_bisection_complete;
  int32_t dominating_box_flag; // indicate whether there is a dominating BoxBin
  BoxBin box1, box2;
  box1.ll_index = box.ll_index;
  box2.ur_index = box.ur_index;
  // this part of code can be simplified, but after which the code might be unclear
  // cut-line along vertical direction
  if (box.cut_direction_x) {
    flag_bisection_complete =
        box.update_cut_index_white_space(grid_bin_white_space_LUT);
    if (flag_bisection_complete) {
      box1.cut_direction_x = false;
      box2.cut_direction_x = false;
      box1.ur_index = box.cut_ur_index;
      box2.ll_index = box.cut_ll_index;
    } else {
      // if bisection fail in one direction, do bisection in the other direction
      box.cut_direction_x = false;
      flag_bisection_complete =
          box.update_cut_index_white_space(grid_bin_white_space_LUT);
      if (flag_bisection_complete) {
        box1.cut_direction_x = false;
        box2.cut_direction_x = false;
        box1.ur_index = box.cut_ur_index;
        box2.ll_index = box.cut_ll_index;
      }
    }
  } else {
    // cut-line along horizontal direction
    flag_bisection_complete =
        box.update_cut_index_white_space(grid_bin_white_space_LUT);
    if (flag_bisection_complete) {
      box1.cut_direction_x = true;
      box2.cut_direction_x = true;
      box1.ur_index = box.cut_ur_index;
      box2.ll_index = box.cut_ll_index;
    } else {
      box.cut_direction_x = true;
      flag_bisection_complete =
          box.update_cut_index_white_space(grid_bin_white_space_LUT);
      if (flag_bisection_complete) {
        box1.cut_direction_x = true;
        box2.cut_direction_x = true;
        box1.ur_index = box.cut_ur_index;
        box2.ll_index = box.cut_ll_index;
      }
    }
  }
  box1.update_cell_area_white_space(grid_bin_mesh);
  box2.update_cell_area_white_space(grid_bin_mesh);
  //BOOST_LOG_TRIVIAL(info)   << box1.ll_index_ << box1.ur_index_ << "\n";
  //BOOST_LOG_TRIVIAL(info)   << box2.ll_index_ << box2.ur_index_ << "\n";
  //box1.update_all_terminal(grid_bin_matrix);
  //box2.update_all_terminal(grid_bin_matrix);
  // if the white space in one bin is dominating the other, ignore the smaller one
  dominating_box_flag = 0;
  if (double(box1.total_white_space) / double(box.total_white_space)
      <= 0.01) {
    dominating_box_flag = 1;
  }
  if (double(box2.total_white_space) / double(box.total_white_space)
      <= 0.01) {
    dominating_box_flag = 2;
  }

  box1.update_boundaries(grid_bin_mesh);
  box2.update_boundaries(grid_bin_mesh);

  if (dominating_box_flag == 0) {
    //BOOST_LOG_TRIVIAL(info)   << "cell list size: " << box.cell_list.size() << "\n";
    //box.update_cell_area(block_list);
    //BOOST_LOG_TRIVIAL(info)   << "total_cell_area: " << box.total_cell_area << "\n";
    box.update_cut_point_cell_list_low_high(
        box1.total_white_space,
        box2.total_white_space
    );
    box1.cell_list = box.cell_list_low;
    box2.cell_list = box.cell_list_high;
    box1.ll_point = box.ll_point;
    box2.ur_point = box.ur_point;
    box1.ur_point = box.cut_ur_point;
    box2.ll_point = box.cut_ll_point;
    box1.total_cell_area = box.total_cell_area_low;
    box2.total_cell_area = box.total_cell_area_high;

    if (box1.ll_index == box1.ur_index) {
      box1.UpdateFixedBlkList(grid_bin_mesh);
      box1.UpdateObsBoundary();
    }
    if (box2.ll_index == box2.ur_index) {
      box2.UpdateFixedBlkList(grid_bin_mesh);
      box2.UpdateObsBoundary();
    }

    /*if ((box1.left < LEFT) || (box1.bottom < BOTTOM)) {
  BOOST_LOG_TRIVIAL(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
  BOOST_LOG_TRIVIAL(info)   << box1.left << " " << box1.bottom << "\n";
}
if ((box2.left < LEFT) || (box2.bottom < BOTTOM)) {
  BOOST_LOG_TRIVIAL(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
  BOOST_LOG_TRIVIAL(info)   << box2.left << " " << box2.bottom << "\n";
}*/

    queue_box_bin.push(box1);
    queue_box_bin.push(box2);
    //box1.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
    //box2.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
    //box1.write_cell_region("first_cell_bounding_box.txt");
    //box2.write_cell_region("first_cell_bounding_box.txt");
  } else if (dominating_box_flag == 1) {
    box2.ll_point = box.ll_point;
    box2.ur_point = box.ur_point;
    box2.cell_list = box.cell_list;
    box2.total_cell_area = box.total_cell_area;
    if (box2.ll_index == box2.ur_index) {
      box2.UpdateFixedBlkList(grid_bin_mesh);
      box2.UpdateObsBoundary();
    }

    /*if ((box2.left < LEFT) || (box2.bottom < BOTTOM)) {
  BOOST_LOG_TRIVIAL(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
  BOOST_LOG_TRIVIAL(info)   << box2.left << " " << box2.bottom << "\n";
}*/

    queue_box_bin.push(box2);
    //box2.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
    //box2.write_cell_region("first_cell_bounding_box.txt");
  } else {
    box1.ll_point = box.ll_point;
    box1.ur_point = box.ur_point;
    box1.cell_list = box.cell_list;
    box1.total_cell_area = box.total_cell_area;
    if (box1.ll_index == box1.ur_index) {
      box1.UpdateFixedBlkList(grid_bin_mesh);
      box1.UpdateObsBoundary();
    }

    /*if ((box1.left < LEFT) || (box1.bottom < BOTTOM)) {
  BOOST_LOG_TRIVIAL(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
  BOOST_LOG_TRIVIAL(info)   << box1.left << " " << box1.bottom << "\n";
}*/

    queue_box_bin.push(box1);
    //box1.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
    //box1.write_cell_region("first_cell_bounding_box.txt");
  }
}

/****
 * keep splitting the biggest box to many small boxes, and keep update the shape
 * of each box and cells should be assigned to the box
 * @return true if succeed, false if fail
 */
bool LookAheadLegalizer::RecursiveBisectionBlockSpreading() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  while (!queue_box_bin.empty()) {
    //std::cout << queue_box_bin.size() << "\n";
    if (queue_box_bin.empty()) break;
    BoxBin &box = queue_box_bin.front();
    // start moving cells to the box, if
    // (a) the box is a grid bin box or a smaller box
    // (b) and with no fixed macros inside
    if (box.ll_index == box.ur_index) {
      //UpdateGridBinBlocks(box);
      if (box.IsContainFixedBlk()) { // if there is a fixed macro inside a box, keep splitting the box
        SplitGridBox(box);
        queue_box_bin.pop();
        continue;
      }
      /* if no terminals inside a box, do cell placement inside the box */
      //PlaceBlkInBoxBisection(box);
      PlaceBlkInBox(box);
      //RoughLegalBlkInBox(box);
    } else {
      SplitBox(box);
    }
    queue_box_bin.pop();
  }

  elapsed_time.RecordEndTime();
  recursive_bisection_block_spreading_time_ += elapsed_time.GetWallTime();
  return true;
}

double LookAheadLegalizer::RemoveCellOverlap() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  ClearGridBinFlag();
  UpdateGridBinState();
  UpdateClusterList();
  do {
    UpdateLargestCluster();
    FindMinimumBoxForLargestCluster();
    RecursiveBisectionBlockSpreading();
    //BOOST_LOG_TRIVIAL(info) << "cluster count: " << cluster_set.size() << "\n";
  } while (!cluster_set.empty());

  //LGTetrisEx legalizer_;
  //legalizer_.TakeOver(this);
  //legalizer_.StartPlacement();

  double evaluate_result_x = ckt_ptr_->WeightedHPWLX();
  upper_bound_hpwl_x_.push_back(evaluate_result_x);
  double evaluate_result_y = ckt_ptr_->WeightedHPWLY();
  upper_bound_hpwl_y_.push_back(evaluate_result_y);
  BOOST_LOG_TRIVIAL(debug) << "Look-ahead legalization complete\n";

  elapsed_time.RecordEndTime();
  tot_lal_time += elapsed_time.GetWallTime();

  if (should_save_intermediate_result_) {
    std::string file_name = "lal_result_" + std::to_string(cur_iter_) + ".txt";
    ++cur_iter_;
    ckt_ptr_->GenMATLABTable(file_name);
    //DumpLookAheadDisplacement("displace_" + std::to_string(cur_iter_), 1);
  }

  BOOST_LOG_TRIVIAL(debug)
    << "(UpdateGridBinState time: " << update_grid_bin_state_time_ << "s)\n";
  BOOST_LOG_TRIVIAL(debug)
    << "(UpdateClusterList time: " << update_cluster_list_time_ << "s)\n";
  BOOST_LOG_TRIVIAL(debug)
    << "(FindMinimumBoxForLargestCluster time: "
    << find_minimum_box_for_largest_cluster_time_ << "s)\n";
  BOOST_LOG_TRIVIAL(debug)
    << "(RecursiveBisectionBlockSpreading time: "
    << recursive_bisection_block_spreading_time_ << "s)\n";

  upper_bound_hpwl_.push_back(evaluate_result_x + evaluate_result_y);
  return upper_bound_hpwl_.back();
}

double LookAheadLegalizer::GetTime() {
  return tot_lal_time;
}

void LookAheadLegalizer::Close() {
  grid_bin_mesh.clear();
  grid_bin_white_space_LUT.clear();
}

} // dali
