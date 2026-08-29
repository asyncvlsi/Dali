/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/delay_line_reservation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <set>
#include <unordered_set>
#include <utility>

namespace dali {

namespace delay_line_reservation_detail {

struct SourceComponent {
  int component_id = -1;
  int llx = 0;
  int lly = 0;
  int width = 0;
  int height = 0;
  PlaceStatus status = UNPLACED;
};

bool Overlaps(int llx, int lly, int urx, int ury,
              const DelayLineReservation& other) {
  return llx < other.urx && other.llx < urx && lly < other.ury &&
         other.lly < ury;
}

bool Fits(const Circuit& circuit, int llx, int lly, int urx, int ury) {
  return llx >= circuit.RegionLLX() && lly >= circuit.RegionLLY() &&
         urx <= circuit.RegionURX() && ury <= circuit.RegionURY();
}

/**
 * Name both lines in a reservation conflict, and say how large each one is.
 *
 * The bare "reservations overlap in the placement region" said which stage
 * refused but not which geometry to look at, so a run that shaped eight lines
 * at once reported the same sentence whichever pair collided. The sizes are
 * here because the usual cause is a line grown by timing feedback until it no
 * longer shares a row range with its neighbour, and the width and height say
 * that directly.
 */
std::string OverlapError(const std::string& name,
                         const DelayLineReservation& reserved, int width,
                         int height) {
  return "delay-line reservations overlap in the placement region: '" + name +
         "' (" + std::to_string(width) + " x " + std::to_string(height) +
         " grid units) collides with already-reserved '" + reserved.name + "'";
}

}  // namespace delay_line_reservation_detail

namespace delay_line_reservation_detail {

/** The grid sites one component covers at a given translation. */
void AppendComponentSites(const SourceComponent& source, int shift_x,
                          int shift_y, std::set<std::pair<int, int>>* sites) {
  for (int y = source.lly + shift_y; y < source.lly + shift_y + source.height;
       ++y) {
    for (int x = source.llx + shift_x; x < source.llx + shift_x + source.width;
         ++x) {
      sites->insert({x, y});
    }
  }
}

/** Sites and right edges of every non-movable cell outside any delay line. */
struct ObstacleMap {
  std::set<std::pair<int, int>> sites;
  std::set<int> right_edges;
};

ObstacleMap CollectObstacles(
    const Circuit& circuit,
    const std::unordered_set<int>& delay_line_component_ids) {
  ObstacleMap obstacles;
  for (const Component& component : circuit.Components()) {
    if (component.IsMovable()) continue;
    if (delay_line_component_ids.count(component.Id()) != 0) continue;
    SourceComponent source;
    source.llx = static_cast<int>(std::round(component.LLX()));
    source.lly = static_cast<int>(std::round(component.LLY()));
    source.width = component.Width();
    source.height = component.Height();
    AppendComponentSites(source, 0, 0, &obstacles.sites);
    obstacles.right_edges.insert(source.llx + source.width);
  }
  return obstacles;
}

}  // namespace delay_line_reservation_detail

/**
 * Plan all delay-line translations without changing the circuit.
 *
 * Conflicts are decided on the sites the cells actually cover, not on their
 * bounding boxes. A folded line occupies two rows and spans its whole
 * separation, so its box is mostly empty: on width-64 bd_pipeline the eight
 * lines produced 18 box intersections and zero shared sites, and every one of
 * those 18 was a refusal of a placement in which nothing touched anything.
 * Interlocking sparse lines are the normal case here, not a hazard.
 *
 * Zero translation is tried first, so a line that is already where it belongs
 * stays there. The fallback keeps the previous behaviour -- slide right past a
 * line that is genuinely in the way -- but now only engages on a real shared
 * site, and every candidate is validated against exact footprints, the
 * placement region per component, and the fixed obstacles the legalizer may not
 * move.
 *
 * Nothing is mutated: the plan is returned whole or not at all.
 */
DelayLineReservationPlan PlanDelayLineReservations(
    const Circuit& circuit,
    const std::vector<DelayLineReservationRequest>& requests) {
  using delay_line_reservation_detail::AppendComponentSites;
  using delay_line_reservation_detail::CollectObstacles;
  using delay_line_reservation_detail::SourceComponent;

  DelayLineReservationPlan plan;
  std::unordered_set<int> planned_component_ids;
  for (const DelayLineReservationRequest& request : requests) {
    planned_component_ids.insert(request.component_ids.begin(),
                                 request.component_ids.end());
  }
  const delay_line_reservation_detail::ObstacleMap obstacles =
      CollectObstacles(circuit, planned_component_ids);

  planned_component_ids.clear();
  // Sites already claimed by lines planned earlier in this call, each carrying
  // the line that claimed it so a refusal can name both sides of the conflict.
  std::map<std::pair<int, int>, std::string> reserved_sites;

  for (const DelayLineReservationRequest& request : requests) {
    if (request.component_ids.empty()) {
      plan.error = "delay-line '" + request.name + "' has no components";
      return plan;
    }

    std::vector<SourceComponent> source_components;
    source_components.reserve(request.component_ids.size());
    int min_x = std::numeric_limits<int>::max();
    int min_y = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::lowest();
    int max_y = std::numeric_limits<int>::lowest();
    bool has_non_movable_component = false;

    for (int component_id : request.component_ids) {
      if (component_id < 0 ||
          component_id >= static_cast<int>(circuit.Components().size())) {
        plan.error = "delay-line '" + request.name +
                     "' references an invalid component id";
        return plan;
      }
      if (!planned_component_ids.insert(component_id).second) {
        plan.error = "component " + std::to_string(component_id) +
                     " belongs to more than one delay-line reservation";
        return plan;
      }
      const Component& component = circuit.Components()[component_id];
      const int llx = static_cast<int>(std::round(component.LLX()));
      const int lly = static_cast<int>(std::round(component.LLY()));
      source_components.push_back({component_id, llx, lly, component.Width(),
                                   component.Height(), component.Status()});
      min_x = std::min(min_x, llx);
      min_y = std::min(min_y, lly);
      max_x = std::max(max_x, llx + component.Width());
      max_y = std::max(max_y, lly + component.Height());
      has_non_movable_component |= !component.IsMovable();
    }

    // A shape wider or taller than the region can never fit, at any
    // translation. Said before any candidate is tried, because "outside the
    // placement region" reads as a position problem and this one is not.
    if (max_x - min_x > circuit.RegionURX() - circuit.RegionLLX() ||
        max_y - min_y > circuit.RegionURY() - circuit.RegionLLY()) {
      plan.error = "delay-line '" + request.name +
                   "' reservation does not fit in the placement region";
      return plan;
    }

    // A line's own cells must not stand on each other. Checked before any
    // translation, because no translation of a rigid shape can fix it.
    std::set<std::pair<int, int>> own_sites;
    int own_area = 0;
    for (const SourceComponent& source : source_components) {
      own_area += source.width * source.height;
      AppendComponentSites(source, 0, 0, &own_sites);
    }
    if (static_cast<int>(own_sites.size()) != own_area) {
      plan.error = "delay-line '" + request.name +
                   "' places more than one component on the same site";
      return plan;
    }

    // Zero first, then the smallest slide that clears a line genuinely in the
    // way, in increasing order. There is deliberately no wrap to the region's
    // left edge: that was the previous fallback, and it relocates a line across
    // the die into a different part of the datapath to buy space, which also
    // let an out-of-region component and a fixed-obstacle collision be
    // "solved" by teleporting rather than reported.
    // Containment in y is a single rigid shift, exactly as before: the search
    // below varies x only, so a line that hangs over the top or bottom edge is
    // brought inside once here or not at all. Dropping this rejected a width-1
    // line 210 um tall that had always simply been slid down into the region.
    int shift_y = 0;
    if (!has_non_movable_component) {
      if (min_y < circuit.RegionLLY()) {
        shift_y = circuit.RegionLLY() - min_y;
      } else if (max_y > circuit.RegionURY()) {
        shift_y = circuit.RegionURY() - max_y;
      }
    }

    std::vector<int> candidate_shifts = {0};
    if (!has_non_movable_component) {
      // The smallest shift that brings an out-of-region line inside. Zero when
      // it already is, so this costs nothing in the ordinary case.
      int containment_shift = 0;
      if (min_x < circuit.RegionLLX()) {
        containment_shift = circuit.RegionLLX() - min_x;
      } else if (max_x > circuit.RegionURX()) {
        containment_shift = circuit.RegionURX() - max_x;
      }
      if (containment_shift != 0) candidate_shifts.push_back(containment_shift);
      // Every shift that would clear something actually in the way: an
      // already-reserved line, or a fixed cell legalization may not move. Both
      // kinds block equally, so both contribute candidates.
      std::set<int> offsets;
      for (const DelayLineReservation& reserved : plan.lines) {
        if (reserved.urx > min_x) offsets.insert(reserved.urx - min_x);
      }
      for (int edge : obstacles.right_edges) {
        if (edge > min_x) offsets.insert(edge - min_x);
      }
      candidate_shifts.insert(candidate_shifts.end(), offsets.begin(),
                              offsets.end());
      // Smallest displacement first, so a line that must move makes the least
      // move that works and the choice does not depend on discovery order.
      std::sort(candidate_shifts.begin(), candidate_shifts.end(),
                [](int left, int right) {
                  if (std::abs(left) != std::abs(right)) {
                    return std::abs(left) < std::abs(right);
                  }
                  return left < right;
                });
      candidate_shifts.erase(
          std::unique(candidate_shifts.begin(), candidate_shifts.end()),
          candidate_shifts.end());
    }

    bool placed = false;
    int chosen_shift_x = 0;
    std::set<std::pair<int, int>> chosen_sites;
    std::string refusal;
    for (int shift_x : candidate_shifts) {
      std::set<std::pair<int, int>> sites;
      bool inside_region = true;
      for (const SourceComponent& source : source_components) {
        if (source.llx + shift_x < circuit.RegionLLX() ||
            source.lly + shift_y < circuit.RegionLLY() ||
            source.llx + shift_x + source.width > circuit.RegionURX() ||
            source.lly + shift_y + source.height > circuit.RegionURY()) {
          inside_region = false;
          break;
        }
        AppendComponentSites(source, shift_x, shift_y, &sites);
      }
      if (!inside_region) {
        if (refusal.empty()) {
          refusal = "delay-line '" + request.name +
                    "' reservation is outside the placement region";
        }
        continue;
      }
      bool collides = false;
      for (const std::pair<int, int>& site : sites) {
        if (obstacles.sites.count(site) != 0) {
          collides = true;
          refusal = "delay-line '" + request.name +
                    "' overlaps a non-movable component that legalization may "
                    "not move";
          break;
        }
        const auto owner = reserved_sites.find(site);
        if (owner != reserved_sites.end()) {
          collides = true;
          if (refusal.empty() || has_non_movable_component) {
            refusal =
                "delay-line '" + request.name + "' (" +
                std::to_string(max_x - min_x) + " x " +
                std::to_string(max_y - min_y) +
                " grid units) shares site " + std::to_string(site.first) + "," +
                std::to_string(site.second) + " with already-reserved '" +
                owner->second + "'";
            if (has_non_movable_component) {
              refusal += ", and contains a non-movable component so it cannot "
                         "be translated clear";
            }
          }
          break;
        }
      }
      if (collides) continue;
      placed = true;
      chosen_shift_x = shift_x;
      chosen_sites = std::move(sites);
      break;
    }

    if (!placed) {
      plan.error = refusal.empty()
                       ? "delay-line '" + request.name +
                             "' has no legal reservation position"
                       : refusal;
      return plan;
    }

    DelayLineReservation reservation;
    reservation.name = request.name;
    reservation.llx = min_x + chosen_shift_x;
    reservation.lly = min_y + shift_y;
    reservation.urx = max_x + chosen_shift_x;
    reservation.ury = max_y + shift_y;
    reservation.components.reserve(source_components.size());
    for (const SourceComponent& source : source_components) {
      reservation.components.push_back(
          {source.component_id, source.llx + chosen_shift_x,
           source.lly + shift_y, source.status});
    }
    for (const std::pair<int, int>& site : chosen_sites) {
      reserved_sites.emplace(site, request.name);
    }
    plan.lines.push_back(std::move(reservation));
  }

  return plan;
}

DelayLineOccupancyReport MeasureDelayLineOccupancy(
    const Circuit& circuit,
    const std::vector<DelayLineReservationRequest>& requests) {
  DelayLineOccupancyReport report;
  // Occupied grid sites per line, and the owning line of every site, so a
  // shared site is attributed rather than merely counted.
  std::vector<std::set<std::pair<int, int>>> occupancy(requests.size());
  std::set<std::pair<int, int>> delay_line_sites;

  for (std::size_t index = 0; index < requests.size(); ++index) {
    const DelayLineReservationRequest& request = requests[index];
    DelayLineOccupancy measured;
    measured.name = request.name;
    measured.component_count = static_cast<int>(request.component_ids.size());
    int min_x = std::numeric_limits<int>::max();
    int min_y = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::lowest();
    int max_y = std::numeric_limits<int>::lowest();
    std::set<int> occupied_rows;

    for (int component_id : request.component_ids) {
      if (component_id < 0 ||
          component_id >= static_cast<int>(circuit.Components().size())) {
        continue;
      }
      const Component& component = circuit.Components()[component_id];
      const int llx = static_cast<int>(std::round(component.LLX()));
      const int lly = static_cast<int>(std::round(component.LLY()));
      const int width = component.Width();
      const int height = component.Height();
      min_x = std::min(min_x, llx);
      min_y = std::min(min_y, lly);
      max_x = std::max(max_x, llx + width);
      max_y = std::max(max_y, lly + height);
      measured.has_non_movable |= !component.IsMovable();
      if (llx < circuit.RegionLLX() || lly < circuit.RegionLLY() ||
          llx + width > circuit.RegionURX() ||
          lly + height > circuit.RegionURY()) {
        measured.inside_region = false;
      }
      for (int y = lly; y < lly + height; ++y) {
        occupied_rows.insert(y);
        for (int x = llx; x < llx + width; ++x) {
          if (!occupancy[index].insert({x, y}).second) {
            ++measured.duplicate_sites;
          }
        }
      }
    }
    measured.llx = min_x;
    measured.lly = min_y;
    measured.urx = max_x;
    measured.ury = max_y;
    measured.occupied_sites = static_cast<int>(occupancy[index].size());
    measured.rows_occupied = static_cast<int>(occupied_rows.size());
    measured.rows_spanned = max_y - min_y;
    report.duplicate_sites += measured.duplicate_sites;
    report.lines.push_back(std::move(measured));
    delay_line_sites.insert(occupancy[index].begin(), occupancy[index].end());
  }

  for (std::size_t left = 0; left < requests.size(); ++left) {
    for (std::size_t right = left + 1; right < requests.size(); ++right) {
      DelayLineConflict conflict;
      conflict.left = requests[left].name;
      conflict.right = requests[right].name;
      const DelayLineOccupancy& a = report.lines[left];
      const DelayLineOccupancy& b = report.lines[right];
      conflict.bounding_boxes_intersect =
          a.llx < b.urx && b.llx < a.urx && a.lly < b.ury && b.lly < a.ury;
      for (const std::pair<int, int>& site : occupancy[left]) {
        if (occupancy[right].count(site) != 0) ++conflict.shared_sites;
      }
      if (conflict.bounding_boxes_intersect) ++report.bounding_box_intersections;
      if (conflict.shared_sites > 0) ++report.cell_intersections;
      if (conflict.bounding_boxes_intersect || conflict.shared_sites > 0) {
        report.conflicts.push_back(std::move(conflict));
      }
    }
  }

  // Any non-movable, non-delay-line cell standing on a site a line occupies.
  std::set<int> delay_line_components;
  for (const DelayLineReservationRequest& request : requests) {
    delay_line_components.insert(request.component_ids.begin(),
                                 request.component_ids.end());
  }
  for (const Component& component : circuit.Components()) {
    if (component.IsMovable()) continue;
    if (delay_line_components.count(component.Id()) != 0) continue;
    const int llx = static_cast<int>(std::round(component.LLX()));
    const int lly = static_cast<int>(std::round(component.LLY()));
    for (int y = lly; y < lly + component.Height(); ++y) {
      for (int x = llx; x < llx + component.Width(); ++x) {
        if (delay_line_sites.count({x, y}) != 0) {
          ++report.obstacle_collisions;
        }
      }
    }
  }
  return report;
}

}  // namespace dali
