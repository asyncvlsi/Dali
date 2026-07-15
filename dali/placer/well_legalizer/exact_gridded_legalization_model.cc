/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
#include "dali/placer/well_legalizer/exact_gridded_legalization_model.h"

#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace dali {

std::string ExactGriddedLegalizationModel::Validate() const {
  if (!std::isfinite(distance_scale_x) || distance_scale_x <= 0.0 ||
      !std::isfinite(distance_scale_y) || distance_scale_y <= 0.0) {
    return "distance scales must be finite and positive";
  }

  std::unordered_map<int, const ExactGriddedStripe*> stripes_by_id;
  stripes_by_id.reserve(stripes.size());
  for (const ExactGriddedStripe& stripe : stripes) {
    if (stripe.stripe_id < 0) return "stripe ids must be non-negative";
    if (!stripes_by_id.emplace(stripe.stripe_id, &stripe).second) {
      return "stripe ids must be unique";
    }
    if (stripe.lx >= stripe.ux || stripe.ly >= stripe.uy) {
      return "stripe rectangles must have positive width and height";
    }
    if (stripe.maximum_rows <= 0) {
      return "stripe maximum row counts must be positive";
    }
    if (stripe.left_boundary_margin < 0 || stripe.right_boundary_margin < 0) {
      return "stripe boundary margins must be non-negative";
    }
    if (stripe.left_boundary_margin + stripe.right_boundary_margin >=
        stripe.ux - stripe.lx) {
      return "stripe boundary margins consume all horizontal capacity";
    }
    if (stripe.minimum_p_well_height < 0 || stripe.minimum_n_well_height < 0) {
      return "stripe minimum well heights must be non-negative";
    }
  }

  std::unordered_set<int> component_ids;
  component_ids.reserve(cells.size());
  for (const ExactGriddedCell& cell : cells) {
    if (cell.component_id < 0) return "component ids must be non-negative";
    if (!component_ids.insert(cell.component_id).second) {
      return "component ids must be unique";
    }
    if (cell.width <= 0) return "component widths must be positive";
    if (cell.regions.empty()) {
      return "every component must contain at least one well region";
    }
    for (const ExactGriddedCellRegion& region : cell.regions) {
      if (region.p_well_height <= 0 || region.n_well_height <= 0) {
        return "component P/N-well heights must be positive";
      }
    }
    if (cell.candidate_stripe_ids.empty()) {
      return "every component must have at least one candidate stripe";
    }
    std::unordered_set<int> candidate_ids;
    for (int stripe_id : cell.candidate_stripe_ids) {
      auto stripe_it = stripes_by_id.find(stripe_id);
      if (stripe_it == stripes_by_id.end()) {
        return "component refers to an unknown candidate stripe";
      }
      if (!candidate_ids.insert(stripe_id).second) {
        return "component candidate stripe ids must be unique";
      }
      if (cell.regions.size() >
          static_cast<size_t>(stripe_it->second->maximum_rows)) {
        return "component has more regions than a candidate stripe has rows";
      }
    }
  }

  for (const ExactGriddedNet& net : nets) {
    if (!std::isfinite(net.weight) || net.weight < 0.0) {
      return "net weights must be finite and non-negative";
    }
    for (const ExactGriddedNetPin& pin : net.pins) {
      if (!std::isfinite(pin.offset_x_n) || !std::isfinite(pin.offset_y_n) ||
          !std::isfinite(pin.offset_x_fs) || !std::isfinite(pin.offset_y_fs) ||
          !std::isfinite(pin.fixed_x) || !std::isfinite(pin.fixed_y)) {
        return "net pin coordinates must be finite";
      }
      if (pin.component_id >= 0 && component_ids.count(pin.component_id) == 0) {
        return "net pin refers to an unknown component";
      }
    }
  }
  return "";
}

}  // namespace dali
