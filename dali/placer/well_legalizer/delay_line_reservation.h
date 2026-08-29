/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_DELAY_LINE_RESERVATION_H_
#define DALI_PLACER_WELL_LEGALIZER_DELAY_LINE_RESERVATION_H_

#include <string>
#include <vector>

#include "dali/circuit/circuit.h"

namespace dali {

/** The component ids that form one registered delay-line chain. */
struct DelayLineReservationRequest {
  std::string name;
  std::vector<int> component_ids;
};

/** A component's proposed position and pre-reservation placement status. */
struct DelayLineReservationComponent {
  int component_id = -1;
  int llx = 0;
  int lly = 0;
  PlaceStatus original_status = UNPLACED;
};

/** One complete, validated delay-line reservation. */
struct DelayLineReservation {
  std::string name;
  int llx = 0;
  int lly = 0;
  int urx = 0;
  int ury = 0;
  std::vector<DelayLineReservationComponent> components;
};

/** A plan is either complete or has no usable reservations. */
struct DelayLineReservationPlan {
  std::vector<DelayLineReservation> lines;
  std::string error;

  bool valid() const { return error.empty(); }
};

/**
 * Plan all delay-line translations without changing the circuit.
 *
 * Non-movable members can remain anchors only when their current placement is
 * already a legal, non-overlapping reservation. Otherwise the complete plan
 * fails before the caller can mutate any component.
 */
DelayLineReservationPlan PlanDelayLineReservations(
    const Circuit& circuit,
    const std::vector<DelayLineReservationRequest>& requests);

/** One line's exact occupancy, independent of any bounding box. */
struct DelayLineOccupancy {
  std::string name;
  int component_count = 0;
  int occupied_sites = 0;
  int duplicate_sites = 0;
  int llx = 0;
  int lly = 0;
  int urx = 0;
  int ury = 0;
  int rows_occupied = 0;
  int rows_spanned = 0;
  bool inside_region = true;
  bool has_non_movable = false;
};

/** Bounding-box versus actual-cell conflict between two lines. */
struct DelayLineConflict {
  std::string left;
  std::string right;
  bool bounding_boxes_intersect = false;
  int shared_sites = 0;
};

/** Everything measured about a set of reservation requests, changing nothing. */
struct DelayLineOccupancyReport {
  std::vector<DelayLineOccupancy> lines;
  std::vector<DelayLineConflict> conflicts;
  int bounding_box_intersections = 0;
  int cell_intersections = 0;
  int duplicate_sites = 0;
  int obstacle_collisions = 0;
};

/**
 * Measure what the lines actually occupy, with no planning and no mutation.
 *
 * A folded delay line occupies two rows and spans `separation` of them, so its
 * bounding box can be almost entirely empty. Whether two lines conflict is
 * therefore a question about occupied sites, and a box test cannot answer it --
 * which is what this exists to establish rather than assume.
 */
DelayLineOccupancyReport MeasureDelayLineOccupancy(
    const Circuit& circuit,
    const std::vector<DelayLineReservationRequest>& requests);

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_DELAY_LINE_RESERVATION_H_
