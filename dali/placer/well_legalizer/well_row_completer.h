/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_WELL_ROW_COMPLETER_H_
#define DALI_PLACER_WELL_LEGALIZER_WELL_ROW_COMPLETER_H_

#include <map>
#include <tuple>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Parameters needed to complete legalized rows with boundary cells. */
struct WellRowCompletionConfig {
  Macro* well_tap_macro = nullptr;
  int well_tap_count_per_row = 2;
  int space_to_well_tap = 0;
  int pre_end_cap_width = 0;
  int post_end_cap_width = 0;
};

/**
 * Materializes well taps and end caps after ordinary-cell legalization.
 *
 * The legalizer reserves the required row margins before clustering. This
 * class then places boundary cells at deterministic locations without adding
 * them to the ordinary-cell legalization problem.
 */
class WellRowCompleter {
 public:
  WellRowCompleter(Circuit* circuit, std::vector<ClusterStripe>* columns,
                   WellRowCompletionConfig config);

  /** Insert well taps into the reserved left and right row margins. */
  void InsertWellTaps();

  /** Create and insert end caps into the reserved row-boundary margins. */
  void InsertEndCaps();

 private:
  using RowHeight = std::tuple<int, int>;

  /** Create one pre/post end-cap macro pair for each unique row height. */
  void CreateEndCapMacros();

  Circuit* circuit_ = nullptr;
  std::vector<ClusterStripe>* columns_ = nullptr;
  WellRowCompletionConfig config_;
  std::map<RowHeight, int> pre_end_cap_macro_ids_;
  std::map<RowHeight, int> post_end_cap_macro_ids_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_WELL_ROW_COMPLETER_H_
