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

/**
 * @file
 * Legalizer used by the multi-height legalization application.
 *
 * Separate from GriddedCellWellLegalizer, which is what the main flow uses. Only
 * the `mhlg` application constructs this; if that application goes, so does
 * this file.
 */
#include "gridded_row_legalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <unordered_map>

#include "dali/common/elapsed_time.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/common/memory.h"
#include "dali/placer/well_legalizer/optimization_helper.h"
#include "dali/placer/well_legalizer/stripe_helper.h"

namespace dali {

void GriddedRowLegalizer::CheckWellInfo() {
  for (auto& macro : ckt_ptr_->tech().Macros()) {
    if (macro.HasWellInfo()) {
      macro.CheckLegality();
    }
  }

  Tech& tech = ckt_ptr_->tech();
  WellLayer& n_well_layer = tech.NwellLayer();
  double grid_value_x = ckt_ptr_->GridValueX();
  int same_spacing = std::ceil(n_well_layer.Spacing() / grid_value_x);
  int op_spacing = std::ceil(n_well_layer.OppositeSpacing() / grid_value_x);
  well_spacing_ = std::max(same_spacing, op_spacing);

  well_tap_p_height_ = well_tap_macro_->PwellHeight(0);
  well_tap_n_height_ = well_tap_macro_->NwellHeight(0);
}

void GriddedRowLegalizer::SetNumThreads(int number_of_threads) {
  DaliExpects(number_of_threads > 0, "negative threads?");
  number_of_threads_ = number_of_threads;
}

void GriddedRowLegalizer::SetExternalSpacePartitioner(
    SpacePartitioner* p_external_partitioner) {
  space_partitioner_ = p_external_partitioner;
}

void GriddedRowLegalizer::SetPartitionMode(int partitioning_mode) {
  partitioning_mode_ = partitioning_mode;
}

void GriddedRowLegalizer::SetMaxRowWidth(double max_row_width) {
  LOG(info) << "Provided max row width: " << max_row_width << " um\n";
  LOG(info) << "Gridded value x: " << ckt_ptr_->GridValueX() << " um\n";
  if (max_row_width < 0) {
    max_row_width_ = RegionRight() - RegionLeft();
  } else {
    max_row_width_ = std::floor(max_row_width / ckt_ptr_->GridValueX());
  }
  LOG(info) << "Max row width in grid unit : " << max_row_width_ << "\n";
}

void GriddedRowLegalizer::PartitionSpaceAndComponents() {
  bool is_external_partitioner_provided = (space_partitioner_ != nullptr);
  if (!is_external_partitioner_provided) {
    space_partitioner_ = new WellSpacePartitioner;
  }

  space_partitioner_->SetCircuit(ckt_ptr_);
  space_partitioner_->SetOutput(&col_list_);
  space_partitioner_->SetReservedSpaceToBoundaries(well_spacing_, well_spacing_,
                                                   1, 1);
  space_partitioner_->SetPartitionMode(partitioning_mode_);
  space_partitioner_->SetMaxRowWidth(max_row_width_);
  space_partitioner_->StartPartitioning();

  if (!is_external_partitioner_provided) {
    delete space_partitioner_;
    space_partitioner_ = nullptr;
  }
}

void GriddedRowLegalizer::SetWellTapCellParameters(
    bool is_well_tap_needed, bool is_checker_board_mode,
    double tap_cell_interval_microns, std::string const& well_tap_macro_name) {
  SetWellTapCellNecessary(is_well_tap_needed);
  SetWellTapCellPlacementMode(is_checker_board_mode);
  /** Set the tap pitch in grid units, derived from MaxPlugDist. */
  SetWellTapCellInterval(tap_cell_interval_microns);
  SetWellTapMacro(well_tap_macro_name);
}

void GriddedRowLegalizer::PrecomputeWellTapCellLocation() {
  if (!is_well_tap_needed_) return;
  for (StripeColumn& cluster : col_list_) {
    for (Stripe& stripe : cluster.stripe_list_) {
      stripe.PrecomputeWellTapCellLocation(
          is_checker_board_mode_, tap_cell_interval_grid_, well_tap_macro_);
    }
  }
}

void GriddedRowLegalizer::InitializeComponentAuxiliaryInfo() {
  auto& components = ckt_ptr_->Components();
  component_auxs_.reserve(components.size());
  for (Component& component : components) {
    if (IsDummyComponent(component)) continue;
    component_auxs_.emplace_back(&component);
  }
}

void GriddedRowLegalizer::SaveInitialLoc() {
  is_init_loc_cached_ = true;
  auto& components = ckt_ptr_->Components();
  for (Component& component : components) {
    if (IsDummyComponent(component)) continue;
    auto aux_ptr = static_cast<ComponentLegalizationState*>(component.AuxPtr());
    aux_ptr->StoreCurLocAsInitLoc();
  }
}

void GriddedRowLegalizer::SaveUpDownLoc() {
  is_greedy_loc_cached_ = true;
  auto& components = ckt_ptr_->Components();
  for (Component& component : components) {
    if (IsDummyComponent(component)) continue;
    auto aux_ptr = static_cast<ComponentLegalizationState*>(component.AuxPtr());
    aux_ptr->StoreCurLocAsGreedyLoc();
  }
}

void GriddedRowLegalizer::SaveConsensusLoc() {
  is_cons_loc_cached_ = true;
  auto& components = ckt_ptr_->Components();
  for (Component& component : components) {
    if (IsDummyComponent(component)) continue;
    auto aux_ptr = static_cast<ComponentLegalizationState*>(component.AuxPtr());
    aux_ptr->StoreCurLocAsConsLoc();
  }
}

void GriddedRowLegalizer::RestoreInitialLocX() {
  DaliExpects(is_init_loc_cached_,
              "Initial locations are not saved, no way to restore");
  auto& components = ckt_ptr_->Components();
  for (Component& component : components) {
    if (IsDummyComponent(component)) continue;
    auto aux_ptr = static_cast<ComponentLegalizationState*>(component.AuxPtr());
    aux_ptr->RecoverInitLocX();
  }
}

void GriddedRowLegalizer::RestoreGreedyLocX() {
  DaliExpects(is_greedy_loc_cached_,
              "Greedy locations are not saved, no way to restore");
  auto& components = ckt_ptr_->Components();
  for (Component& component : components) {
    if (IsDummyComponent(component)) continue;
    auto aux_ptr = static_cast<ComponentLegalizationState*>(component.AuxPtr());
    aux_ptr->RecoverGreedyLocX();
  }
}

void GriddedRowLegalizer::RestoreConsensusLocX() {
  DaliExpects(is_cons_loc_cached_,
              "Consensus locations are not saved, no way to restore");
  auto& components = ckt_ptr_->Components();
  for (Component& component : components) {
    if (IsDummyComponent(component)) continue;
    auto aux_ptr = static_cast<ComponentLegalizationState*>(component.AuxPtr());
    aux_ptr->RecoverConsLocX();
  }
}

void GriddedRowLegalizer::SetLegalizationMaxIteration(int max_iteration) {
  greedy_max_iter_ = max_iteration;
}

bool GriddedRowLegalizer::InitializeAssignedRows(
    std::vector<StripeColumn> &columns, int p_height, int n_height,
    int left_margin, int right_margin, std::size_t *reassigned) {
  if (reassigned)
    *reassigned = 0;
  if (p_height < 0 || n_height < 0 || left_margin < 0 || right_margin < 0)
    return false;
  well_tap_p_height_ = p_height;
  well_tap_n_height_ = n_height;
  // Nearest-stripe partitioning can select a sliver around fixed macros that
  // cannot contain every occupied region. Repair only incompatible owners,
  // reserving complete-region demand before invoking the legacy row builder.
  // Coordinates/poses remain unchanged during this assignment transaction.
  struct Available {
    Stripe *stripe;
    long long spare;
    std::vector<Component *> assigned;
  };
  struct Pending {
    Component *cell;
    Stripe *source;
    long long height, demand;
  };
  std::vector<Available> available;
  std::vector<Pending> pending;
  std::unordered_set<Component *> seen;
  auto height_for = [&](Component *cell) -> long long {
    long long total = 0;
    for (int r = 0; r < cell->MacroPtr()->RegionCount(); ++r)
      total += std::max(p_height,
                        cell->MacroPtr()->PwellHeight(r, cell->IsFlipped())) +
               std::max(n_height,
                        cell->MacroPtr()->NwellHeight(r, cell->IsFlipped()));
    return total;
  };
  const int minimum_row_height = std::max(1, p_height + n_height);
  for (auto &col : columns)
    for (auto &stripe : col.stripe_list_) {
      const long long row_height_budget = std::max(0, stripe.Height()) /
                                          minimum_row_height *
                                          minimum_row_height;
      Available entry{&stripe,
                      static_cast<long long>(std::max(
                          0, stripe.Width() - left_margin - right_margin)) *
                          row_height_budget,
                      {}};
      for (auto *cell : stripe.component_ptrs_vec_) {
        if (!cell || !seen.insert(cell).second || !cell->IsMovable() ||
            !cell->MacroPtr()->HasCompleteWellRegions())
          return false;
        const auto needed = height_for(cell), demand = needed * cell->Width();
        if (cell->Width() <= stripe.Width() - left_margin - right_margin &&
            needed <= stripe.Height()) {
          entry.assigned.push_back(cell);
          entry.spare -= demand;
        } else
          pending.push_back({cell, &stripe, needed, demand});
      }
      // Fractional leftover row height cannot accommodate a well band. If the
      // compatible owners already exceed the complete-row area lower bound,
      // reassign the largest region demands before considering any proposals.
      if (entry.spare < 0) {
        std::sort(entry.assigned.begin(), entry.assigned.end(),
                  [&](Component *a, Component *b) {
                    auto ah = height_for(a), bh = height_for(b);
                    if (ah != bh)
                      return ah > bh;
                    if (a->Width() != b->Width())
                      return a->Width() > b->Width();
                    return a->Id() < b->Id();
                  });
        std::size_t remove = 0;
        while (entry.spare < 0 && remove < entry.assigned.size()) {
          auto *cell = entry.assigned[remove++];
          const auto h = height_for(cell), demand = h * cell->Width();
          pending.push_back({cell, &stripe, h, demand});
          entry.spare += demand;
        }
        entry.assigned.erase(entry.assigned.begin(),
                             entry.assigned.begin() + remove);
      }
      available.push_back(std::move(entry));
    }
  std::sort(pending.begin(), pending.end(),
            [](const Pending &a, const Pending &b) {
              if (a.height != b.height)
                return a.height > b.height;
              if (a.demand != b.demand)
                return a.demand > b.demand;
              return a.cell->Id() < b.cell->Id();
            });
  for (auto move : pending) {
    Available *target = nullptr;
    double best = std::numeric_limits<double>::infinity();
    for (auto &candidate : available) {
      auto &s = *candidate.stripe;
      if (candidate.spare < move.demand || move.height > s.Height() ||
          move.cell->Width() > s.Width() - left_margin - right_margin)
        continue;
      const double x =
          std::clamp(move.cell->LLX(), double(s.LLX() + left_margin),
                     double(s.URX() - right_margin - move.cell->Width()));
      const double y = std::clamp(move.cell->LLY(), double(s.LLY()),
                                  double(s.URY() - move.height));
      const double distance =
          std::abs(x - move.cell->LLX()) + std::abs(y - move.cell->LLY());
      if (distance < best) {
        best = distance;
        target = &candidate;
      }
    }
    if (!target)
      return false;
    target->assigned.push_back(move.cell);
    target->spare -= move.demand;
  }
  for (auto &entry : available) {
    entry.stripe->component_ptrs_vec_ = std::move(entry.assigned);
    entry.stripe->component_count_ = entry.stripe->component_ptrs_vec_.size();
  }
  if (reassigned)
    *reassigned = pending.size();
  if (!pending.empty())
    LOG(info) << "Multi-region incompatible-stripe assignments repaired: "
              << pending.size() << "\n";
  std::unordered_map<Component *, std::unordered_set<Stripe *>> rejected;
  std::unordered_set<Component *> changed;
  for (auto move : pending)
    changed.insert(move.cell);
  // Actual multi-deck fragmentation can still defeat an area lower bound.
  // Spill only owners the row builder could not place, then rebuild all
  // affected rows. Every rejected owner/stripe pair is excluded from subsequent
  // targets. A fixed geometry-derived pass bound prevents an infeasible input
  // spinning.
  for (std::size_t pass = 0; pass <= 2 * available.size(); ++pass) {
    bool complete = true;
    for (auto &entry : available) {
      auto &stripe = *entry.stripe;
      if (stripe.component_ptrs_vec_.empty()) {
        stripe.gridded_rows_.clear();
        continue;
      }
      stripe.well_tap_cell_location_even_.clear();
      if (left_margin)
        stripe.well_tap_cell_location_even_.emplace_back(
            stripe.LLX(), stripe.LLX() + left_margin);
      if (right_margin)
        stripe.well_tap_cell_location_even_.emplace_back(
            stripe.URX() - right_margin, stripe.URX());
      stripe.well_tap_cell_location_odd_ = stripe.well_tap_cell_location_even_;
      std::vector<Component *> spill;
      if (!StripeLegalizationUpward(stripe, false, true, &spill)) {
        if (spill.empty())
          return false;
        complete = false;
        for (auto *cell : spill) {
          if (!rejected[cell].insert(&stripe).second)
            return false;
          const auto h = height_for(cell), demand = h * cell->Width();
          Available *target = nullptr;
          double best = std::numeric_limits<double>::infinity();
          for (auto &candidate : available) {
            auto &s = *candidate.stripe;
            if (&s == &stripe || rejected[cell].count(&s) ||
                candidate.spare < demand || h > s.Height() ||
                cell->Width() > s.Width() - left_margin - right_margin)
              continue;
            const double x =
                std::clamp(cell->LLX(), double(s.LLX() + left_margin),
                           double(s.URX() - right_margin - cell->Width()));
            const double y =
                std::clamp(cell->LLY(), double(s.LLY()), double(s.URY() - h));
            const double distance =
                std::abs(x - cell->LLX()) + std::abs(y - cell->LLY());
            if (distance < best) {
              best = distance;
              target = &candidate;
            }
          }
          if (!target)
            return false;
          auto &owners = stripe.component_ptrs_vec_;
          owners.erase(std::find(owners.begin(), owners.end(), cell));
          target->stripe->component_ptrs_vec_.push_back(cell);
          entry.spare += demand;
          target->spare -= demand;
          changed.insert(cell);
          stripe.component_count_ = owners.size();
          target->stripe->component_count_ =
              target->stripe->component_ptrs_vec_.size();
        }
        if (reassigned)
          *reassigned = changed.size();
        break;
      }
      stripe.CleanUpTemporaryRowSegments();
      for (auto &row : stripe.gridded_rows_)
        row.SetBoundaryMargins(left_margin, right_margin);
    }
    if (complete) {
      if (reassigned)
        *reassigned = changed.size();
      return true;
    }
  }
  return false;
}

bool GriddedRowLegalizer::StripeLegalizationUpward(
    Stripe &stripe, bool use_init_loc, bool fit_all,
    std::vector<Component *> *spill) {
  if (spill)
    spill->clear();
  stripe.gridded_rows_.clear();
  stripe.front_id_ = -1;
  stripe.is_bottom_up_ = true;

  if (fit_all) {
    std::sort(stripe.component_ptrs_vec_.begin(),
              stripe.component_ptrs_vec_.end(), [](Component *a, Component *b) {
                int ah = a->MacroPtr()->RegionCount(),
                    bh = b->MacroPtr()->RegionCount();
                if (ah != bh)
                  return ah > bh;
                if (a->LLY() != b->LLY())
                  return a->LLY() < b->LLY();
                if (a->LLX() != b->LLX())
                  return a->LLX() < b->LLX();
                return a->Id() < b->Id();
              });
  } else {
    stripe.SortComponentsBasedOnYLocation(0);
  }

  size_t processed_component_count = 0;
  while (processed_component_count < stripe.component_ptrs_vec_.size()) {
    if (fit_all && stripe.front_id_ + 1 >=
                       stripe.Height() / std::max(1, well_tap_p_height_ +
                                                         well_tap_n_height_)) {
      if (spill)
        spill->assign(stripe.component_ptrs_vec_.begin() +
                          processed_component_count,
                      stripe.component_ptrs_vec_.end());
      LOG(warning) << "Multi-region row capacity exhausted: remaining owners="
                   << stripe.component_ptrs_vec_.size() -
                          processed_component_count
                   << "\n";
      for (auto &row : stripe.gridded_rows_) {
        long long occupied = 0;
        for (auto cr : row.ComponentRegions())
          occupied += cr.component->Width();
        LOG(warning) << "  row y=" << row.LLY() << " height=" << row.Height()
                     << " occupied width=" << occupied << "\n";
      }
      return false;
    }
    stripe.UpdateFrontClusterUpward(well_tap_p_height_, well_tap_n_height_);
    processed_component_count = stripe.FitComponentsToFrontSpaceUpward(
        processed_component_count, greedy_cur_iter_, fit_all);
    stripe.LegalizeFrontCluster(use_init_loc);
  }
  stripe.UpdateRemainingClusters(well_tap_p_height_, well_tap_n_height_, true);
  stripe.UpdateComponentYLocation();
  stripe.UpdateComponentStretchLength();

  const bool fits = stripe.HasNoRowsSpillingOut();
  if (!fits && spill) {
    std::unordered_set<Component *> overflow;
    for (auto &row : stripe.gridded_rows_)
      if (row.LLY() < stripe.LLY() || row.URY() > stripe.URY())
        for (auto cr : row.ComponentRegions())
          overflow.insert(cr.component);
    for (auto *cell : stripe.component_ptrs_vec_)
      if (overflow.count(cell))
        spill->push_back(cell);
  }
  return fits;
}

bool GriddedRowLegalizer::StripeLegalizationDownward(Stripe &stripe,
                                                     bool use_init_loc) {
  stripe.gridded_rows_.clear();
  stripe.front_id_ = -1;
  stripe.is_bottom_up_ = false;

  stripe.SortComponentsBasedOnYLocation(2);

  size_t processed_component_count = 0;
  while (processed_component_count < stripe.component_ptrs_vec_.size()) {
    stripe.UpdateFrontClusterDownward(well_tap_p_height_, well_tap_n_height_);
    processed_component_count = stripe.FitComponentsToFrontSpaceDownward(
        processed_component_count, greedy_cur_iter_);
    stripe.LegalizeFrontCluster(use_init_loc);
  }
  stripe.UpdateRemainingClusters(well_tap_p_height_, well_tap_n_height_, false);
  stripe.UpdateComponentYLocation();
  stripe.UpdateComponentStretchLength();

  return stripe.HasNoRowsSpillingOut();
}

void GriddedRowLegalizer::CleanUpTemporaryRowSegments() {
  for (StripeColumn& col : col_list_) {
    for (Stripe& stripe : col.stripe_list_) {
      stripe.CleanUpTemporaryRowSegments();
    }
  }
}

/** Legalize each stripe by trying upward then downward, keeping the better.
 * @return true if all stripes fit. */
bool GriddedRowLegalizer::UpwardDownwardLegalization(bool use_init_loc) {
  LOG(info) << "Start upward-downward legalization\n";
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  bool res = true;
  for (StripeColumn& col : col_list_) {
    bool is_success = true;
    for (Stripe& stripe : col.stripe_list_) {
      stripe.max_disp_ = ckt_ptr_->AverageComponentWidth();
      bool is_from_bottom = true;
      for (greedy_cur_iter_ = 0; greedy_cur_iter_ < greedy_max_iter_;
           ++greedy_cur_iter_) {
        if (is_from_bottom) {
          is_success = StripeLegalizationUpward(stripe, use_init_loc);
        } else {
          is_success = StripeLegalizationDownward(stripe, use_init_loc);
        }
        is_from_bottom = !is_from_bottom;
        if (is_success) {
          break;
        }
      }
      res = res && is_success;
    }
  }
  CleanUpTemporaryRowSegments();
  ReportDisplacement();

  elapsed_time.RecordEndTime();
  elapsed_time.PrintTimeElapsed();
  return res;
}

bool GriddedRowLegalizer::StripeLegalizationUpwardWithDispCheck(
    Stripe& stripe, bool use_init_loc) {
  stripe.gridded_rows_.clear();
  stripe.front_id_ = -1;
  stripe.is_bottom_up_ = true;

  stripe.SortComponentsBasedOnYLocation(0);

  size_t processed_component_count = 0;
  while (processed_component_count < stripe.component_ptrs_vec_.size()) {
    stripe.UpdateFrontClusterUpward(well_tap_p_height_, well_tap_n_height_);
    processed_component_count =
        stripe.FitComponentsToFrontSpaceUpwardWithDispCheck(
            processed_component_count, greedy_cur_iter_);
    stripe.LegalizeFrontCluster(use_init_loc);
  }
  stripe.UpdateRemainingClusters(well_tap_p_height_, well_tap_n_height_, true);
  stripe.UpdateComponentYLocation();
  stripe.UpdateComponentStretchLength();

  return stripe.HasNoRowsSpillingOut();
}

bool GriddedRowLegalizer::StripeLegalizationDownwardWithDispCheck(
    Stripe& stripe, bool use_init_loc) {
  (void)stripe;
  (void)use_init_loc;
  DaliExpects(false, "to be implemented");
  return true;
}

/** Upward/downward legalization under a per-cell displacement limit. */
bool GriddedRowLegalizer::UpwardDownwardLegalizationWithDispCheck(
    bool use_init_loc) {
  LOG(info)
      << "Start upward-downward legalization with displacement checking\n";
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  bool res = true;
  for (StripeColumn& col : col_list_) {
    bool is_success = true;
    for (Stripe& stripe : col.stripe_list_) {
      stripe.max_disp_ = ckt_ptr_->AverageComponentWidth();
      bool is_from_bottom = true;
      for (greedy_cur_iter_ = 0; greedy_cur_iter_ < greedy_max_iter_;
           ++greedy_cur_iter_) {
        if (is_from_bottom) {
          is_success =
              StripeLegalizationUpwardWithDispCheck(stripe, use_init_loc);
        } else {
          is_success =
              StripeLegalizationDownwardWithDispCheck(stripe, use_init_loc);
        }
        is_from_bottom = !is_from_bottom;
        if (is_success) {
          break;
        }
      }
      res = res && is_success;
    }
  }
  CleanUpTemporaryRowSegments();
  ReportDisplacement();

  elapsed_time.RecordEndTime();
  elapsed_time.PrintTimeElapsed();
  return res;
}

bool GriddedRowLegalizer::IsLeftmostPlacementLegal() {
  bool res = true;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      bool tmp_res = stripe.IsLeftmostPlacementLegal();
      res = res && tmp_res;
    }
  }
  return res;
}

bool GriddedRowLegalizer::IsPlacementLegal() {
  bool res = true;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      bool tmp_res = stripe.IsStripeLegal();
      res = res && tmp_res;
    }
  }
  return res;
}

bool GriddedRowLegalizer::IterativeDisplacementOptimization() {
  LOG(info) << "Optimizing displacement X using the consensus algorithm\n";
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      stripe.IterativeCellReordering(consensus_max_iter_, number_of_threads_);
    }
  }

  elapsed_time.RecordEndTime();
  elapsed_time.PrintTimeElapsed();

  ReportDisplacement();

  return IsPlacementLegal();
}

/** Instantiate the precomputed tap locations as real components. */
void GriddedRowLegalizer::EmbodyWellTapCells() {
  if (!is_well_tap_needed_) return;

  size_t well_tap_cell_count_upper_limit = 0;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      size_t upper_limit_per_row =
          std::max(stripe.well_tap_cell_location_even_.size(),
                   stripe.well_tap_cell_location_odd_.size());
      well_tap_cell_count_upper_limit +=
          upper_limit_per_row * stripe.gridded_rows_.size();
    }
  }
  auto& tap_cell_collection = ckt_ptr_->design().WellTapComponentCollection();
  tap_cell_collection.Clear();
  tap_cell_collection.Reserve(well_tap_cell_count_upper_limit);

  // create well-tap cells
  size_t counter = 0;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      counter = stripe.AddWellTapCells(ckt_ptr_, well_tap_macro_, counter);
    }
  }
  tap_cell_collection.Freeze();
}

/** Log how far cells moved from their pre-legalization locations. */
void GriddedRowLegalizer::ReportDisplacement() {
  if (!is_init_loc_cached_) {
    LOG(info)
        << "Initial locations are not saved, cannot compute displacement\n";
  }
  double disp_x = 0, disp_y = 0;
  double quadratic_disp_x = 0, quadratic_disp_y = 0;
  int measured_component_count = 0;
  auto& components = ckt_ptr_->Components();
  for (Component& component : components) {
    if (IsDummyComponent(component)) continue;
    auto aux_ptr = static_cast<ComponentLegalizationState*>(component.AuxPtr());
    double2d init_loc = aux_ptr->InitLoc();
    double tmp_disp_x = std::fabs(component.LLX() - init_loc.x);
    double tmp_disp_y = std::fabs(component.LLY() - init_loc.y);
    disp_x += tmp_disp_x;
    disp_y += tmp_disp_y;
    quadratic_disp_x += tmp_disp_x * tmp_disp_x;
    quadratic_disp_y += tmp_disp_y * tmp_disp_y;
    ++measured_component_count;
  }
  if (measured_component_count == 0) {
    LOG(info) << "Skip displacement report: no components were measured\n";
    return;
  }
  disp_x *= ckt_ptr_->GridValueX();
  disp_y *= ckt_ptr_->GridValueY();
  quadratic_disp_x *= ckt_ptr_->GridValueX() * ckt_ptr_->GridValueX();
  quadratic_disp_y *= ckt_ptr_->GridValueY() * ckt_ptr_->GridValueY();
  auto count = static_cast<double>(measured_component_count);
  LOG(info) << "  Current linear displacement\n";
  LOG(info) << "    x: " << disp_x << "(" << disp_x / count << ")"
            << ", y: " << disp_y << "(" << disp_y / count << ")"
            << ", sum: " << disp_x + disp_y << "(" << (disp_x + disp_y) / count
            << ")"
            << " um\n";
  LOG(info) << "  Current quadratic displacement\n";
  LOG(info) << "    x: " << quadratic_disp_x << "(" << quadratic_disp_x / count
            << ")"
            << ", y: " << quadratic_disp_y << "(" << quadratic_disp_y / count
            << ")"
            << ", sum: " << quadratic_disp_x + quadratic_disp_y << "("
            << (quadratic_disp_x + quadratic_disp_y) / count << ")"
            << " um^2\n";
}

/** Run multi-height legalization end to end.
 * @return true on success. */
bool GriddedRowLegalizer::StartPlacement() {
  PrintStartStatement("gridded row well legalization");

  bool is_successful = true;

  CheckWellInfo();
  PartitionSpaceAndComponents();
  PrecomputeWellTapCellLocation();

  InitializeComponentAuxiliaryInfo();
  SaveInitialLoc();

  // bool is_success = UpwardDownwardLegalization();
  bool is_success = UpwardDownwardLegalization();
  SaveUpDownLoc();
  ReportHPWL();
  ReportEffectiveDensity();

  if (is_success) {
    RestoreInitialLocX();
    IterativeDisplacementOptimization();
    ReportHPWL();
    SaveConsensusLoc();
    ReportOutOfBoundCell();
    UpwardDownwardLegalization(true);
    ReportHPWL();

    EmbodyWellTapCells();
  }

  PrintEndStatement("gridded row well legalization", is_success);

  return is_successful;
}

void GriddedRowLegalizer::ImportStandardRowSegments(phydb::PhyDB& phydb) {
  // create one stripe column and one stripe
  col_list_.emplace_back();
  auto& col = col_list_.back();
  col.stripe_list_.emplace_back();
  auto& stripe = col.stripe_list_.back();
  stripe.ImportStandardRowSegments(phydb, *ckt_ptr_);
  auto& components = ckt_ptr_->Components();
  stripe.component_ptrs_vec_.reserve(components.size());
  for (auto& component : components) {
    if (IsDummyComponent(component)) continue;
    stripe.component_ptrs_vec_.emplace_back(&component);
  }
}

void GriddedRowLegalizer::AssignStandardCellsToRowSegments() {
  auto& stripe = col_list_[0].stripe_list_[0];
  stripe.AssignStandardCellsToRowSegments();
}

/** Log cell displacement for the standard-cell path of this legalizer. */
void GriddedRowLegalizer::ReportStandardCellDisplacement() {
  if (!is_init_loc_cached_) {
    LOG(info)
        << "Initial locations are not saved, cannot compute displacement\n";
  }
  double sum_disp_x = 0, sum_disp_y = 0;
  double max_disp_x = 0, max_disp_y = 0;
  double max_manhattan_disp = 0;
  double sum_eucli_disp = 0;
  double max_euclidean_disp = 0;
  int cell_count = 0;
  auto& components = ckt_ptr_->Components();
  for (Component& component : components) {
    if (IsDummyComponent(component)) continue;
    auto aux_ptr = static_cast<ComponentLegalizationState*>(component.AuxPtr());
    double2d init_loc = aux_ptr->InitLoc();
    double tmp_disp_x = std::fabs(component.LLX() - init_loc.x);
    double tmp_disp_y = std::fabs(component.LLY() - init_loc.y);
    sum_disp_x += tmp_disp_x;
    sum_disp_y += tmp_disp_y;
    max_disp_x = std::max(max_disp_x, tmp_disp_x);
    max_disp_y = std::max(max_disp_y, tmp_disp_y);
    max_manhattan_disp = std::max(max_manhattan_disp, tmp_disp_x + tmp_disp_y);

    double tmp_euclidean_disp =
        std::sqrt(tmp_disp_x * tmp_disp_x + tmp_disp_y * tmp_disp_y);
    sum_eucli_disp += tmp_euclidean_disp;
    max_euclidean_disp = std::max(max_euclidean_disp, tmp_euclidean_disp);
    ++cell_count;
  }
  LOG(info) << "Standard cell legalization summary\n";
  LOG(info) << "  sum manhattan displacement\n";
  LOG(info) << "    x: " << sum_disp_x << ", y: " << sum_disp_y
            << ", sum: " << sum_disp_x + sum_disp_y << " sites\n";
  LOG(info) << "  average manhattan displacement\n";
  LOG(info) << "    x: " << sum_disp_x / cell_count
            << ", y: " << sum_disp_y / cell_count
            << ", sum: " << (sum_disp_x + sum_disp_y) / cell_count
            << " sites\n";
  LOG(info) << "  max manhattan displacement\n";
  LOG(info) << "    x: " << max_disp_x << ", y: " << max_disp_y << " sites\n";
  LOG(info) << "  max sum manhattan displacement: " << max_manhattan_disp
            << " sites\n";

  LOG(info) << "  sum euclidean displacement: " << sum_eucli_disp << " sites\n";
  LOG(info) << "  average euclidean displacement: "
            << sum_eucli_disp / cell_count << " sites\n";
  LOG(info) << "  max euclidean displacement: " << max_euclidean_disp
            << " sites\n";
}

bool GriddedRowLegalizer::StartStandardLegalization() {
  AssignStandardCellsToRowSegments();
  IterativeDisplacementOptimization();
  ReportHPWL();
  ReportBoundingBox();
  return true;
}

void GriddedRowLegalizer::ReportOutOfBoundCell() {
  size_t cnt = 0;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      cnt += stripe.OutOfBoundCell();
    }
  }
  LOG(info) << cnt << " cells out of the corresponding cluster boundary!\n";
}

void GriddedRowLegalizer::ReportEffectiveDensity() {
  long long tot_eff_area = 0;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      for (auto& row : stripe.gridded_rows_) {
        for (auto& component : row.ComponentRegions()) {
          tot_eff_area += component.component->Width() * row.Height();
        }
      }
    }
  }

  double eff_density = static_cast<double>(tot_eff_area) /
                       static_cast<double>(RegionWidth()) /
                       static_cast<double>(RegionHeight()) * 100;
  LOG(info) << "Effective density: " << eff_density << "%\n";
}

void GriddedRowLegalizer::SetWellTapCellNecessary(bool is_well_tap_needed) {
  is_well_tap_needed_ = is_well_tap_needed;
  std::string is_needed = is_well_tap_needed_ ? "True" : "False";
  LOG(info) << "Place well tap cells: " << is_needed << "\n";
}

void GriddedRowLegalizer::SetWellTapCellPlacementMode(
    bool is_checker_board_mode) {
  is_checker_board_mode_ = is_checker_board_mode;
  std::string is_mode_on = is_checker_board_mode_ ? "True" : "False";
  LOG(info) << "Checkerboard mode on: " << is_mode_on << "\n";
}

/** Set the tap pitch in grid units, derived from MaxPlugDist. */
void GriddedRowLegalizer::SetWellTapCellInterval(
    double tap_cell_interval_microns) {
  if (tap_cell_interval_microns > 0) {
    LOG(info) << "Provided well tap cell interval: "
              << tap_cell_interval_microns << " um\n";
  } else {
    Tech& tech = ckt_ptr_->tech();
    WellLayer& n_well_layer = tech.NwellLayer();
    double max_unplug_length = n_well_layer.MaxPlugDist();
    if (is_checker_board_mode_) {
      tap_cell_interval_microns = 4 * max_unplug_length;
      LOG(info) << "Using default well tap cell interval 4*max_unplug_length: "
                << tap_cell_interval_microns << " um\n";
    } else {
      tap_cell_interval_microns = 2 * max_unplug_length;
      LOG(info) << "Using default well tap cell interval 2*max_unplug_length: "
                << tap_cell_interval_microns << " um\n";
    }
  }
  LOG(info) << "Gridded value x: " << ckt_ptr_->GridValueX() << " um\n";
  tap_cell_interval_grid_ =
      std::floor(tap_cell_interval_microns / ckt_ptr_->GridValueX());
  LOG(info) << "Well tap cell interval in grid unit : "
            << tap_cell_interval_grid_ << "\n";
}

void GriddedRowLegalizer::SetWellTapMacro(
    std::string const& well_tap_macro_name) {
  if (well_tap_macro_name.empty()) {
    LOG(info) << "Well tap cell type not specified\n";
    DaliExpects(!ckt_ptr_->tech().WellTapMacroIds().empty(),
                "No well tap cells provided in the cell library?");
    int well_tap_macro_id = ckt_ptr_->tech().WellTapMacroIds()[0];
    well_tap_macro_ = &(ckt_ptr_->tech().Macros()[well_tap_macro_id]);
    LOG(info) << "Using the default well tap cell: " << well_tap_macro_->Name()
              << "\n";
  } else {
    LOG(info) << "Provided well tap macro: " << well_tap_macro_name << "\n";
    well_tap_macro_ = ckt_ptr_->GetMacroPtr(well_tap_macro_name);
  }
}

}  // namespace dali
