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
#ifndef DALI_PLACER_DETAILED_PLACER_DETAILED_PLACER_H_
#define DALI_PLACER_DETAILED_PLACER_DETAILED_PLACER_H_

#include <vector>

#include "dali/circuit/component.h"
#include "dali/circuit/row.h"
#include "dali/placer/placer.h"

namespace dali {

/**
 * Wirelength-driven detailed placer for already legalized standard-cell rows.
 *
 * This starts with the local re-ordering technique from the ISPD 2005 detailed
 * placement paper: for each small window of consecutive cells in a row segment,
 * enumerate all left-to-right orders and keep the best HPWL-improving order.
 */
class DetailedPlacer : public Placer {
 public:
  bool StartPlacement() override;

 private:
  static constexpr int kLocalReorderWindowSize = 3;

  double WindowWireLengthCost(const std::vector<Component*>& components,
                              int start, int window_size);
  void PlaceWindow(const std::vector<Component*>& order, int left_bound,
                   int right_bound);
  bool ReorderWindow(std::vector<Component*>* components, int start,
                     int window_size);
  int LocalReorderSegment(GeneralRowSegment* segment, int window_size);
};

}  // namespace dali

#endif  // DALI_PLACER_DETAILED_PLACER_DETAILED_PLACER_H_
