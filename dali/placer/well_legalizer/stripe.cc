/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
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
#include "stripe.h"

#include <algorithm>
#include <climits>

namespace dali {

void Stripe::MinDisplacementAdjustment() {
  for (auto &cluster: cluster_list_) {
    cluster.UpdateMinDisplacementLLY();
  }

  std::sort(
      cluster_list_.begin(),
      cluster_list_.end(),
      [](const Cluster &cluster0, const Cluster &cluster1) {
        return cluster0.MinDisplacementLLY()
            < cluster1.MinDisplacementLLY();
      }
  );

  std::vector<ClusterSegment> segments;

  int sz = (int) cluster_list_.size();
  int lower_bound = ly_;
  int upper_bound = ly_ + height_;
  for (int i = 0; i < sz; ++i) {
    // create a segment which contains only this block
    Cluster &cluster = cluster_list_[i];
    double init_y = cluster.MinDisplacementLLY();
    if (init_y < lower_bound) {
      init_y = lower_bound;
    }
    if (init_y + cluster.Height() > upper_bound) {
      init_y = upper_bound - cluster.Height();
    }
    segments.emplace_back(&cluster, init_y);

    // if this new segment is the only segment, do nothing
    size_t seg_sz = segments.size();
    if (seg_sz == 1) continue;

    // check if this segment overlap with the previous one, if yes, merge these two segments
    // repeats until this is no overlap or only one segment left

    ClusterSegment *cur_seg = &(segments[seg_sz - 1]);
    ClusterSegment *prev_seg = &(segments[seg_sz - 2]);
    while (prev_seg->IsNotOnBottom(*cur_seg)) {
      prev_seg->Merge(*cur_seg, lower_bound, upper_bound);
      segments.pop_back();

      seg_sz = segments.size();
      if (seg_sz == 1) break;
      cur_seg = &(segments[seg_sz - 1]);
      prev_seg = &(segments[seg_sz - 2]);
    }
  }

  for (auto &seg: segments) {
    seg.UpdateClusterLocation();
  }
}

void Stripe::UpdateFrontCluster(int p_height, int n_height) {
  ++front_id_;
  if (front_id_ >= cluster_list_.size()) {
    cluster_list_.emplace_back();
  }
  cluster_list_[front_id_].SetLLX(lx_);
  cluster_list_[front_id_].SetWidth(width_);

  // determine the orientation of lower y of the front cluster
  int ly;
  bool is_orient_N;
  if (front_id_ == 0) {
    ly = ly_;
    is_orient_N = is_first_row_orient_N_;
  } else {
    ly = cluster_list_[front_id_ - 1].URY();
    is_orient_N = !cluster_list_[front_id_ - 1].IsOrientN();
  }
  cluster_list_[front_id_].SetLLY(ly);
  cluster_list_[front_id_].SetOrient(is_orient_N);

  cluster_list_[front_id_].UpdateWellHeightFromBottom(p_height, n_height);
  cluster_list_[front_id_].UpdateSubClusters();
}

/****
 * Add following clusters and set orientation accordingly.
 * Add the corresponding block region into these clusters based on row orientation and block orientation.
 *
 * @param p_blk
 */
void Stripe::SimplyAddFollowingClusters(Block *p_blk) {
  BlockTypeMultiWell *well = p_blk->TypePtr()->MultiWellPtr();
  size_t row_count = well->RowCount();
  for (size_t i = 1; i < row_count; ++i) {
    size_t row_index = front_id_ + i;
    if (row_index >= cluster_list_.size()) {
      cluster_list_.emplace_back();
    }
    bool is_orient_N = !cluster_list_[row_index - 1].IsOrientN();
    cluster_list_[row_index].SetOrient(is_orient_N);
    cluster_list_[row_index].AddBlockRegion(p_blk, i);
  }
}

bool Stripe::AddBlockToFrontCluster(Block *p_blk) {
  bool res;
  res = cluster_list_[front_id_].AttemptToAdd(p_blk);
  if (!res) return false;

  // add this block to other clusters above the front cluster
  SimplyAddFollowingClusters(p_blk);

  return true;
}

size_t Stripe::FitBlocksToFrontSpace(size_t start_id) {
  std::vector<Block *> legalized_blks;
  std::vector<Block *> skipped_blks;

  size_t blks_sz = block_list_.size();
  for (size_t i = start_id; i < blks_sz; ++i) {
    Block *p_blk = block_list_[i];
    if (!cluster_list_[front_id_].IsOverlap(p_blk, 1)) {
      break;
    }
    if (cluster_list_[front_id_].IsOrientMatching(p_blk)) {
      if (AddBlockToFrontCluster(p_blk)) {
        legalized_blks.push_back(p_blk);
      } else {
        break;
      }
    } else {
      skipped_blks.push_back(p_blk);
    }
  }

  // put legalized blocks back to the sorted list
  for (size_t i = 0; i < legalized_blks.size(); ++i) {
    block_list_[i + start_id] = legalized_blks[i];
  }

  // put skipped blocks back to the sorted list
  start_id = start_id + legalized_blks.size();
  for (size_t i = 0; i < skipped_blks.size(); ++i) {
    block_list_[i + start_id] = skipped_blks[i];
  }

  return start_id;
}

void Stripe::LegalizeFrontCluster() {
  cluster_list_[front_id_].SubClusterLegalize();
}

void Stripe::UpdateRemainingClusters(int p_height, int n_height) {
  size_t sz = cluster_list_.size();
  for (size_t i = front_id_ + 1; i < sz; ++i) {
    cluster_list_[i].SetLLX(lx_);
    cluster_list_[i].SetWidth(width_);
    cluster_list_[i].SetLLY(cluster_list_[i - 1].URY());
    cluster_list_[i].RecomputeHeight(p_height, n_height);
  }
}

void Stripe::UpdateBlockStretchLength() {
  for (auto &cluster: cluster_list_) {
    cluster.InitializeBlockStretching();
  }

  size_t sz = cluster_list_.size();
  for (size_t i = 1; i < sz; ++i) {
    Cluster &cur_cluster = cluster_list_[i];
    Cluster &pre_cluster = cluster_list_[i - 1];
    for (auto &blk_region: cur_cluster.blk_regions_) {
      size_t id = blk_region.region_id;
      if (id >= 1) {
        Block *p_blk = blk_region.p_blk;
        BlockTypeMultiWell *well = blk_region.p_blk->TypePtr()->MultiWellPtr();
        --id;
        int well_edge_distance =
            well->AdjacentRegionEdgeDistance(id, p_blk->IsFlipped());
        int actual_edge_distance =
            (cur_cluster.LLY() + cur_cluster.PNEdge()) -
                (pre_cluster.LLY() + pre_cluster.PNEdge());
        int length = actual_edge_distance - well_edge_distance;
        p_blk->SetStretchLength(id, length);
      }
    }
  }
}

Stripe *ClusterStripe::GetStripeMatchSeg(SegI seg, int y_loc) {
  Stripe *res = nullptr;
  for (auto &Stripe: stripe_list_) {
    if ((Stripe.URY() == y_loc) && (Stripe.LLX() == seg.lo)
        && (Stripe.URX() == seg.hi)) {
      res = &Stripe;
      break;
    }
  }
  return res;
}

Stripe *ClusterStripe::GetStripeMatchBlk(Block *blk_ptr) {
  Stripe *res = nullptr;
  double center_x = blk_ptr->X();
  double center_y = blk_ptr->Y();
  for (auto &&Stripe: stripe_list_) {
    if ((Stripe.LLY() <= center_y) &&
        (Stripe.URY() > center_y) &&
        (Stripe.LLX() <= center_x) &&
        (Stripe.URX() > center_x)) {
      res = &Stripe;
      break;
    }
  }
  return res;
}

Stripe *ClusterStripe::GetStripeClosestToBlk(Block *blk_ptr, double &distance) {
  Stripe *res = nullptr;
  double center_x = blk_ptr->X();
  double center_y = blk_ptr->Y();
  double min_distance = DBL_MAX;
  for (auto &Stripe: stripe_list_) {
    double tmp_distance;
    if ((Stripe.LLY() <= center_y) && (Stripe.URY() > center_y) &&
        (Stripe.LLX() <= center_x) && (Stripe.URX() > center_x)) {
      res = &Stripe;
      tmp_distance = 0;
    } else if ((Stripe.LLX() <= center_x) && (Stripe.URX() > center_x)) {
      tmp_distance = std::min(
          std::abs(center_y - Stripe.LLY()),
          std::abs(center_y - Stripe.URY())
      );
    } else if ((Stripe.LLY() <= center_y) && (Stripe.URY() > center_y)) {
      tmp_distance = std::min(
          std::abs(center_x - Stripe.LLX()),
          std::abs(center_x - Stripe.URX())
      );
    } else {
      tmp_distance = std::min(
          std::abs(center_x - Stripe.LLX()),
          std::abs(center_x - Stripe.URX())
      ) + std::min(
          std::abs(center_y - Stripe.LLY()),
          std::abs(center_y - Stripe.URY())
      );
    }
    if (tmp_distance < min_distance) {
      min_distance = tmp_distance;
      res = &Stripe;
    }
  }

  distance = min_distance;
  return res;
}

void ClusterStripe::AssignBlockToSimpleStripe() {
  for (auto &Stripe: stripe_list_) {
    Stripe.block_count_ = 0;
    Stripe.block_list_.clear();
  }

  for (auto &blk_ptr: block_list_) {
    double tmp_dist;
    auto Stripe = GetStripeClosestToBlk(blk_ptr, tmp_dist);
    Stripe->block_count_++;
  }

  for (auto &Stripe: stripe_list_) {
    Stripe.block_list_.reserve(Stripe.block_count_);
  }

  for (auto &blk_ptr: block_list_) {
    double tmp_dist;
    auto Stripe = GetStripeClosestToBlk(blk_ptr, tmp_dist);
    Stripe->block_list_.push_back(blk_ptr);
  }
}

}