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

#ifndef DALI_PLACER_PLACER_H_
#define DALI_PLACER_PLACER_H_

// clang-format off
#include <stdio.h>
#include <common/config.h>
// clang-format on

#include <fstream>
#include <iostream>
#include <string>

#include "dali/circuit/circuit.h"
#include "dali/common/elapsed_time.h"

namespace dali {

/** Base class for placement flows that operate on a Circuit. */
class Placer {
 public:
  Placer();
  Placer(double aspect_ratio, double filling_rate);
  virtual ~Placer() = default;

  /** Load placer options from a configuration file. */
  virtual void LoadConf(std::string const& config_file);

  /** Attach the circuit to be placed. */
  void SetInputCircuit(Circuit* circuit);

  /** Set the number of threads used by derived placers. */
  void SetNumThreads(int num_threads);

  /** Set target placement density. */
  void SetPlacementDensity(double density = 2.0 / 3.0);

  /** Return target placement density. */
  double PlacementDensity() const;

  /** Set placement-region height/width ratio. */
  void SetAspectRatio(double ratio = 1.0);

  /** Return placement-region height/width ratio. */
  double AspectRatio() const;

  /** Set target whitespace-to-block-area ratio. */
  void SetSpaceBlockRatio(double ratio);

  /** Verify that a placement boundary has been configured. */
  void CheckPlacementBoundary();

  /** Derive placement boundary from area, density, and aspect ratio. */
  void SetBoundaryAuto();

  /** Set placement boundary in Dali grid units. */
  void SetBoundary(int left, int right, int bottom, int top);

  /** Copy placement boundary from the attached circuit. */
  void SetBoundaryFromCircuit();

  /** Log current placement boundaries. */
  void ReportBoundaries() const;

  /** Return left placement boundary in Dali grid units. */
  int RegionLeft() const { return left_; }

  /** Return right placement boundary in Dali grid units. */
  int RegionRight() const { return right_; }

  /** Return bottom placement boundary in Dali grid units. */
  int RegionBottom() const { return bottom_; }

  /** Return top placement boundary in Dali grid units. */
  int RegionTop() const { return top_; }

  /** Return placement-region width in Dali grid units. */
  int RegionWidth() const { return right_ - left_; }

  /** Return placement-region height in Dali grid units. */
  int RegionHeight() const { return top_ - bottom_; }

  /** Recompute aspect ratio from the current placement boundary. */
  void UpdateAspectRatio();

  /** Sort block-pin lists on all nets. */
  void NetSortBlkPin();

  /** Run the placement flow. */
  virtual bool StartPlacement();

  /** Return weighted HPWL in x for the attached circuit. */
  double WeightedHPWLX() {
    DaliExpects(ckt_ptr_ != nullptr,
                "No input circuit specified, cannot compute WeightedHPWLX!");
    return ckt_ptr_->WeightedHPWLX();
  }

  /** Return weighted HPWL in y for the attached circuit. */
  double WeightedHPWLY() {
    DaliExpects(ckt_ptr_ != nullptr,
                "No input circuit specified, cannot compute WeightedHPWLY!");
    return ckt_ptr_->WeightedHPWLY();
  }

  /** Return total weighted HPWL for the attached circuit. */
  double WeightedHPWL() {
    DaliExpects(ckt_ptr_ != nullptr,
                "No input circuit specified, cannot compute HPWL!");
    return ckt_ptr_->WeightedHPWL();
  }

  /** Log weighted HPWL for the attached circuit. */
  void ReportHPWL() {
    DaliExpects(ckt_ptr_ != nullptr,
                "No input circuit specified, cannot compute HPWL!");
    ckt_ptr_->ReportHPWL();
  }

  /** Log weighted bounding box for the attached circuit. */
  void ReportBoundingBox() {
    DaliExpects(ckt_ptr_ != nullptr,
                "No input circuit specified, cannot compute bounding box!");
    ckt_ptr_->ReportBoundingBox();
  }

  /** Log center-to-center HPWL for the attached circuit. */
  void ReportHPWLCtoC() {
    DaliExpects(ckt_ptr_ != nullptr,
                "No input circuit specified, cannot compute HPWLCtoC!");
    ckt_ptr_->ReportHPWLCtoC();
  }

  /** Copy common placement state from another placer. */
  void TakeOver(Placer* placer);

  /** Validate target density settings. */
  void CheckTargetDensity() const;

  /** Validate net connectivity for placement. */
  void CheckNets();

  /** Run general placement precondition checks. */
  void SanityCheck();

  /** Mark movable blocks as placed after placement. */
  void UpdateMovableBlkPlacementStatus();

  /** Generate a MATLAB well table from the attached circuit. */
  virtual void GenMATLABWellTable(std::string const& name_of_file,
                                  [[maybe_unused]] int well_emit_mode) {
    ckt_ptr_->GenMATLABWellTable(name_of_file);
  }

  /** Generate a MATLAB script for placed block/net visualization. */
  void GenMATLABScriptPlaced(
      std::string const& name_of_file = "block_net_list.m");

  /** Save Bookshelf terminal and node files. */
  bool SaveNodeTerminal(std::string const& terminal_file = "terminal.txt",
                        std::string const& node_file = "nodes.txt");

  /** Emit DEF well geometry for flows that support it. */
  virtual void EmitDEFWellFile(
      [[maybe_unused]] std::string const& name_of_file,
      [[maybe_unused]] int well_emit_mode,
      [[maybe_unused]] bool enable_emitting_cluster = true);

  /** Shift all blocks in x for testing. */
  void ShiftX(double shift_x);

  /** Shift all blocks in y for testing. */
  void ShiftY(double shift_y);

  /** Return true when blk is a dummy/helper block. */
  bool IsDummyBlock(Block& blk);

 protected:
  /* essential data entries */
  int num_threads_;
  double aspect_ratio_;  // placement region Height/Width
  double placement_density_;

  /* the following entries are derived data
   * note that the following entries can be manually changed
   * if so, the aspect_ratio_ or filling_rate_ might also be changed */
  int left_, right_, bottom_, top_;
  // boundaries of the placement region
  Circuit* ckt_ptr_;

  // record start/end time
  ElapsedTime elapsed_time_;

  double GetBlkHPWL(Block& blk);

  virtual void PrintStartStatement(std::string const& name_of_process);
  virtual void PrintEndStatement(std::string const& name_of_process,
                                 bool is_success);
};

}  // namespace dali

#endif  // DALI_PLACER_PLACER_H_
