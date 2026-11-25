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
 * Fixed-row mixed-height placement. Each tall cell has one pose, and its
 * footprint consumes the same X interval on every row it spans. Reserving
 * those intervals first lets Abacus and detailed placement work on ordinary
 * independent single-height segments.
 */
#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_legalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>

#include "dali/common/logging.h"

namespace dali {
namespace {

using RowIterator = std::vector<StandardCellRow>::const_iterator;

RowIterator FirstRowAtOrAbove(const std::vector<StandardCellRow> &rows, int y) {
  return std::lower_bound(
      rows.begin(), rows.end(), y,
      [](const StandardCellRow &row, int ly) { return row.ly < ly; });
}

bool SameSiteGrid(const StandardCellRow &base, const StandardCellRow &row) {
  return (base.lx - row.lx) % base.site_width == 0;
}

/** Visit all bands of a footprint only if each contains a compatible row. */
bool FindFootprintRows(const StandardCellPlacementModel &model,
                       RowIterator base, int x, int width, int height,
                       std::vector<int> *footprint_rows) {
  const auto &rows = model.Rows();
  footprint_rows->clear();
  if ((x - base->lx) % base->site_width != 0 ||
      !model.IsIntervalFree(static_cast<int>(base - rows.begin()), x,
                            x + width)) {
    return false;
  }
  footprint_rows->push_back(static_cast<int>(base - rows.begin()));
  for (int y = base->Uy(); y < base->ly + height; y += base->height) {
    auto row = FirstRowAtOrAbove(rows, y);
    for (; row != rows.end() && row->ly == y; ++row) {
      if (SameSiteGrid(*base, *row) &&
          model.IsIntervalFree(static_cast<int>(row - rows.begin()), x,
                               x + width)) {
        break;
      }
    }
    if (row == rows.end() || row->ly != y) {
      return false;
    }
    footprint_rows->push_back(static_cast<int>(row - rows.begin()));
  }
  return true;
}

/**
 * Intersect free intervals on every row of a candidate stack. Compatible site
 * grids make every endpoint in the intersection a site on all covered rows.
 */
std::vector<StandardCellFreeSegment>
CommonFreeSegments(const std::vector<StandardCellRow> &rows,
                   const StandardCellRow &base, int width, int height) {
  auto common = base.free_segments;
  std::vector<StandardCellFreeSegment> intersection;
  for (int y = base.Uy(); y < base.ly + height && !common.empty();
       y += base.height) {
    intersection.clear();
    auto row = FirstRowAtOrAbove(rows, y);
    auto first = common.begin();
    for (; row != rows.end() && row->ly == y; ++row) {
      if (!SameSiteGrid(base, *row)) {
        continue;
      }
      for (const auto &segment : row->free_segments) {
        while (first != common.end() && first->ux <= segment.lx) {
          ++first;
        }
        for (auto it = first; it != common.end() && it->lx < segment.ux; ++it) {
          StandardCellFreeSegment overlap{std::max(it->lx, segment.lx),
                                          std::min(it->ux, segment.ux)};
          if (overlap.CanFit(width)) {
            intersection.push_back(overlap);
          }
        }
      }
    }
    common.swap(intersection);
  }
  return common;
}

bool NearInteger(double x) { return std::abs(x - std::round(x)) < 1e-6; }

} // namespace

int StandardCellLegalizer::MixedHeightRowHeight(
    const std::vector<Component *> &components) const {
  if (mixed_height_rows_ == nullptr) {
    return 0;
  }
  int row_height = ckt_ptr_->RowHeightGridUnit();
  auto &rows = mixed_height_rows_->GetRowVec();
  auto &sites = mixed_height_rows_->GetSitesRef();
  if (!rows.empty()) {
    const int site_id = rows.front().GetSiteId();
    if (site_id >= 0 && site_id < static_cast<int>(sites.size())) {
      const double height =
          ckt_ptr_->LengthPhydb2DaliY(sites[site_id].GetHeight());
      if (NearInteger(height) && height > 0) {
        row_height = static_cast<int>(std::round(height));
      }
    }
  }
  return std::any_of(components.begin(), components.end(),
                     [row_height](const Component *cell) {
                       return cell->Height() != row_height;
                     })
             ? row_height
             : 0;
}

/**
 * Import horizontal, nonoverlapping row fragments with a common site size.
 * Clip only at whole sites and keep the DEF origin's site phase in each
 * fragment. Missing bands or gaps between fragments cannot support cells.
 */
bool StandardCellLegalizer::ImportFixedRows(int row_height) {
  auto &sites = mixed_height_rows_->GetSitesRef();
  int site_width = 0;
  for (const auto &input : mixed_height_rows_->GetRowVec()) {
    const int site_id = input.GetSiteId();
    if (site_id < 0 || site_id >= static_cast<int>(sites.size())) {
      LOG(error) << "Mixed-height DEF row has an invalid site: "
                 << input.GetName() << "\n";
      return false;
    }
    const auto &site = sites[site_id];
    const double x = ckt_ptr_->LocPhydb2DaliX(input.GetOriginX());
    const double y = ckt_ptr_->LocPhydb2DaliY(input.GetOriginY());
    const double width = ckt_ptr_->LengthPhydb2DaliX(site.GetWidth());
    const double height = ckt_ptr_->LengthPhydb2DaliY(site.GetHeight());
    const double pitch = input.GetStepX() /
                         (ckt_ptr_->DistanceMicrons() * ckt_ptr_->GridValueX());
    const auto orient = input.GetOrient();
    if (!NearInteger(x) || !NearInteger(y) || !NearInteger(width) ||
        width < 1 || std::abs(height - row_height) >= 1e-6 ||
        (site_width != 0 && std::abs(width - site_width) >= 1e-6) ||
        input.GetNumX() < 1 || input.GetNumY() != 1 ||
        (std::abs(pitch - width) >= 1e-6 &&
         !(input.GetNumX() == 1 && input.GetStepX() == 0)) ||
        (orient != phydb::CompOrient::N && orient != phydb::CompOrient::FS)) {
      LOG(error) << "Unsupported mixed-height DEF row: " << input.GetName()
                 << " (expected on-grid horizontal N/FS rows with a common "
                    "site size and contiguous sites)\n";
      return false;
    }
    site_width = static_cast<int>(std::round(width));
    const int lx = static_cast<int>(std::round(x));
    const int ly = static_cast<int>(std::round(y));
    if (ly < RegionBottom() || ly + row_height > RegionTop()) {
      continue;
    }
    const int first =
        std::max(0, (RegionLeft() - lx + site_width - 1) / site_width);
    const int last =
        std::min(input.GetNumX(), (RegionRight() - lx) / site_width);
    if (first < last) {
      placement_model_.AddRow(lx + first * site_width, ly, row_height,
                              site_width, last - first);
      placement_model_.Rows().back().orient_n = orient == phydb::CompOrient::N;
    }
  }

  auto &rows = placement_model_.Rows();
  if (rows.empty()) {
    LOG(error) << "Mixed-height legalization requires usable DEF rows\n";
    return false;
  }
  std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
    return std::tie(a.ly, a.lx) < std::tie(b.ly, b.lx);
  });
  for (size_t i = 1; i < rows.size(); ++i) {
    const auto &prev = rows[i - 1];
    const auto &row = rows[i];
    if ((row.ly == prev.ly && row.lx < prev.Ux()) ||
        (row.ly > prev.ly && row.ly < prev.Uy())) {
      LOG(error) << "Overlapping row definitions in mixed-height DEF input\n";
      return false;
    }
  }
  mixed_height_site_width_ = site_width;
  return true;
}

/**
 * Pack taller cells first, then wider cells at the same height. Choose the
 * nearest feasible site by physical displacement. No cell poses are changed
 * until tall reservations and the single-height Abacus assignments succeed.
 */
bool StandardCellLegalizer::ReserveTallCells(
    std::vector<Component *> *components, int row_height,
    std::vector<TallPlacement> *tall_placements) {
  for (const auto *cell : *components) {
    if (cell->Height() <= 0 || cell->Height() % row_height != 0 ||
        cell->Width() <= 0 || cell->Width() % mixed_height_site_width_ != 0) {
      LOG(error) << "Cell " << cell->Name()
                 << " does not occupy a whole number of mixed-height sites\n";
      return false;
    }
  }
  std::vector<Component *> tall_cells;
  auto first_tall = std::remove_if(components->begin(), components->end(),
                                   [&](Component *cell) {
                                     if (cell->Height() == row_height) {
                                       return false;
                                     }
                                     tall_cells.push_back(cell);
                                     return true;
                                   });
  components->erase(first_tall, components->end());
  std::sort(tall_cells.begin(), tall_cells.end(),
            [](const Component *a, const Component *b) {
              return std::make_tuple(-a->Height(), -a->Width(), a->Id()) <
                     std::make_tuple(-b->Height(), -b->Width(), b->Id());
            });

  const auto &rows = placement_model_.Rows();
  std::vector<int> footprint;
  for (auto *cell : tall_cells) {
    TallPlacement best{cell, 0, -1};
    double best_cost = std::numeric_limits<double>::infinity();
    const int row_count = static_cast<int>(rows.size());
    int above = static_cast<int>(
        FirstRowAtOrAbove(rows, std::ceil(cell->LLY())) - rows.begin());
    int below = above - 1;
    auto y_distance = [&](int i) {
      return std::abs(rows[i].ly - cell->LLY()) * ckt_ptr_->GridValueY();
    };
    while (below >= 0 || above < row_count) {
      const bool visit_below =
          above == row_count ||
          (below >= 0 && y_distance(below) <= y_distance(above));
      const int i = visit_below ? below-- : above++;
      const auto &row = rows[i];
      const double dy = y_distance(i);
      if (dy >= best_cost) {
        break;
      }
      for (auto segment :
           CommonFreeSegments(rows, row, cell->Width(), cell->Height())) {
        const int site = static_cast<int>(
            std::round((cell->LLX() - segment.lx) / row.site_width));
        const int x = std::clamp(segment.lx + site * row.site_width, segment.lx,
                                 segment.ux - cell->Width());
        const double cost =
            dy + std::abs(x - cell->LLX()) * ckt_ptr_->GridValueX();
        if (cost < best_cost) {
          best = {cell, x, i};
          best_cost = cost;
        }
      }
    }
    if (best.row_index < 0) {
      LOG(error) << "No common free DEF row interval for tall cell "
                 << cell->Name() << "\n";
      return false;
    }
    const bool fits =
        FindFootprintRows(placement_model_, rows.begin() + best.row_index,
                          best.x, cell->Width(), cell->Height(), &footprint);
    DaliExpects(fits, "Tall-cell interval must fit on every covered row");
    for (int row_index : footprint) {
      placement_model_.ReserveInterval(row_index, best.x,
                                       best.x + cell->Width());
    }
    tall_placements->push_back(best);
  }
  LOG(info) << "Mixed-height legalization reserved " << tall_cells.size()
            << " tall cells on fixed DEF rows\n";
  return true;
}

/**
 * Sweep each original free row, before subtracting the tall-cell reservations,
 * to verify actual poses and multi-row overlaps independently of ownership.
 */
bool StandardCellLegalizer::ValidateMixedHeightPlacement() {
  if (mixed_height_site_width_ == 0) {
    return true;
  }
  const auto &rows = fixed_row_model_.Rows();
  std::vector<std::vector<std::pair<int, int>>> occupied(rows.size());
  std::vector<int> footprint;
  for (auto &cell : ckt_ptr_->Components()) {
    if (cell.IsFixed() || IsDummyComponent(cell)) {
      continue;
    }
    const int x = static_cast<int>(std::round(cell.LLX()));
    const int y = static_cast<int>(std::round(cell.LLY()));
    auto row = FirstRowAtOrAbove(rows, y);
    if (cell.LLX() != x || cell.LLY() != y || cell.URY() != y + cell.Height()) {
      LOG(error) << "Non-grid mixed-height footprint: " << cell.Name() << "\n";
      return false;
    }
    for (; row != rows.end() && row->ly == y; ++row) {
      if (FindFootprintRows(fixed_row_model_, row, x, cell.Width(),
                            cell.Height(), &footprint)) {
        break;
      }
    }
    if (row == rows.end() || row->ly != y ||
        OrientForRow(static_cast<int>(row - rows.begin())) != cell.Orient()) {
      LOG(error) << "Mixed-height footprint outside compatible DEF rows: "
                 << cell.Name() << "\n";
      return false;
    }
    for (int i : footprint) {
      occupied[i].emplace_back(x, x + cell.Width());
    }
  }
  for (size_t i = 0; i < rows.size(); ++i) {
    auto &intervals = occupied[i];
    std::sort(intervals.begin(), intervals.end());
    for (size_t j = 1; j < intervals.size(); ++j) {
      if (intervals[j].first < intervals[j - 1].second) {
        LOG(error) << "Overlapping mixed-height footprints at row y="
                   << rows[i].ly << " x=" << intervals[j].first << "\n";
        return false;
      }
    }
  }
  LOG(info) << "Mixed-height row geometry violations: 0\n";
  return true;
}

} // namespace dali
