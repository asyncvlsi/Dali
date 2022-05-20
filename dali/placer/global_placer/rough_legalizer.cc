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

#include "dali/common/logging.h"
#include "dali/common/timing.h"

namespace dali {

RoughLegalizer::RoughLegalizer(Circuit *ckt_ptr) {
  DaliExpects(ckt_ptr != nullptr, "Circuit is a nullptr?");
  ckt_ptr_ = ckt_ptr;
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
  grid_bin_height = static_cast<int>(std::round(std::sqrt(grid_bin_area)));
  grid_bin_width = grid_bin_height;
  grid_cnt_x = std::ceil(double(ckt_ptr_->RegionWidth()) / grid_bin_width);
  grid_cnt_y = std::ceil(double(ckt_ptr_->RegionHeight()) / grid_bin_height);
  BOOST_LOG_TRIVIAL(info)
    << "Global placement bin width, height: "
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
  for (int i = 0; i < grid_cnt_x; i++) {
    for (int j = 0; j < grid_cnt_y; j++) {
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
  for (int i = 0; i < grid_cnt_x; ++i) {
    grid_bin_mesh[i][grid_cnt_y - 1].top = ckt_ptr_->RegionURY();
    grid_bin_mesh[i][grid_cnt_y - 1].white_space =
        grid_bin_mesh[i][grid_cnt_y - 1].Area();
  }
  // make sure the right placement boundary is the same as the right of the rightmost bins
  for (int i = 0; i < grid_cnt_y; ++i) {
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
  auto &blocks = ckt_ptr_->Blocks();
  int sz = static_cast<int>(blocks.size());
  for (int i = 0; i < sz; i++) {
    /* find the left, right, bottom, top index of the grid */
    if (blocks[i].IsMovable()) continue;
    bool fixed_blk_out_of_region = int(blocks[i].LLX()) >= ckt_ptr_->RegionURX()
        || int(blocks[i].URX()) <= ckt_ptr_->RegionLLX()
        || int(blocks[i].LLY()) >= ckt_ptr_->RegionURY()
        || int(blocks[i].URY()) <= ckt_ptr_->RegionLLY();
    if (fixed_blk_out_of_region) continue;
    int left_index = (int) std::floor(
        (blocks[i].LLX() - ckt_ptr_->RegionLLX()) / grid_bin_width);
    int right_index = (int) std::floor(
        (blocks[i].URX() - ckt_ptr_->RegionLLX()) / grid_bin_width);
    int bottom_index = (int) std::floor(
        (blocks[i].LLY() - ckt_ptr_->RegionLLY()) / grid_bin_height);
    int top_index = (int) std::floor(
        (blocks[i].URY() - ckt_ptr_->RegionLLY()) / grid_bin_height);
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
    for (int j = left_index; j <= right_index; ++j) {
      for (int k = bottom_index; k <= top_index; ++k) {
        /* the following case might happen:
         * the top/right of a fixed block overlap with the bottom/left of
         * a grid box if this case happens, we need to ignore this fixed
         * block for this grid box. */
        bool blk_out_of_bin =
            int(blocks[i].LLX() >= grid_bin_mesh[j][k].right) ||
                int(blocks[i].URX() <= grid_bin_mesh[j][k].left) ||
                int(blocks[i].LLY() >= grid_bin_mesh[j][k].top) ||
                int(blocks[i].URY() <= grid_bin_mesh[j][k].bottom);
        if (blk_out_of_bin) continue;
        grid_bin_mesh[j][k].fixed_blocks.push_back(&(blocks[i]));
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
        static_cast<int>(std::round(fixed_blk.LLX())),
        static_cast<int>(std::round(fixed_blk.LLY())),
        static_cast<int>(std::round(fixed_blk.URX())),
        static_cast<int>(std::round(fixed_blk.URY()))
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
  for (int kx = 0; kx < grid_cnt_x; ++kx) {
    for (int ky = 0; ky < grid_cnt_y; ++ky) {
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

  upper_bound_hpwlx_.clear();
  upper_bound_hpwly_.clear();
  upper_bound_hpwl_.clear();
  InitGridBins();
  InitWhiteSpaceLUT();
}
/*
void LookAheadLegalizer::BackUpBlockLocation() {
  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());
  for (int i = 0; i < sz; ++i) {
    x_anchor[i] = block_list[i].LLX();
    y_anchor[i] = block_list[i].LLY();
  }
}

void LookAheadLegalizer::ClearGridBinFlag() {

}

void LookAheadLegalizer::UpdateGridBinState() {

}

void LookAheadLegalizer::UpdateClusterList() {

}
 */

double LookAheadLegalizer::LookAheadLegalization() {
  return 0;
}

} // dali
