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

#include <fstream>
#include <iostream>
#include <string>

#include <common/config.h>

#include "dali/circuit/circuit.h"
#include "dali/common/elapsed_time.h"

namespace dali {

class Placer {
 public:
  Placer();
  Placer(double aspect_ratio, double filling_rate);
  virtual ~Placer() = default;

  virtual void LoadConf(std::string const &config_file);

  void SetInputCircuit(Circuit *circuit);

  void SetNumThreads(int32_t num_threads);

  void SetPlacementDensity(double density = 2.0 / 3.0);
  double PlacementDensity() const;
  void SetAspectRatio(double ratio = 1.0);
  double AspectRatio() const;
  void SetSpaceBlockRatio(double ratio);

  void CheckPlacementBoundary();
  void SetBoundaryAuto();
  void SetBoundary(int32_t left, int32_t right, int32_t bottom, int32_t top);
  void SetBoundaryFromCircuit();
  void ReportBoundaries() const;

  int32_t RegionLeft() const { return left_; }
  int32_t RegionRight() const { return right_; }
  int32_t RegionBottom() const { return bottom_; }
  int32_t RegionTop() const { return top_; }
  int32_t RegionWidth() const { return right_ - left_; }
  int32_t RegionHeight() const { return top_ - bottom_; }

  void UpdateAspectRatio();
  void NetSortBlkPin();
  virtual bool StartPlacement();

  double WeightedHPWLX() {
    DaliExpects(
        ckt_ptr_ != nullptr,
        "No input circuit specified, cannot compute WeightedHPWLX!"
    );
    return ckt_ptr_->WeightedHPWLX();
  }
  double WeightedHPWLY() {
    DaliExpects(
        ckt_ptr_ != nullptr,
        "No input circuit specified, cannot compute WeightedHPWLY!"
    );
    return ckt_ptr_->WeightedHPWLY();
  }
  double WeightedHPWL() {
    DaliExpects(
        ckt_ptr_ != nullptr,
        "No input circuit specified, cannot compute HPWL!"
    );
    return ckt_ptr_->WeightedHPWL();
  }

  void ReportHPWL() {
    DaliExpects(
        ckt_ptr_ != nullptr,
        "No input circuit specified, cannot compute HPWL!"
    );
    ckt_ptr_->ReportHPWL();
  }

  void ReportBoundingBox() {
    DaliExpects(
        ckt_ptr_ != nullptr,
        "No input circuit specified, cannot compute bounding box!"
    );
    ckt_ptr_->ReportBoundingBox();
  }

  void ReportHPWLCtoC() {
    DaliExpects(
        ckt_ptr_ != nullptr,
        "No input circuit specified, cannot compute HPWLCtoC!"
    );
    ckt_ptr_->ReportHPWLCtoC();
  }

  void TakeOver(Placer *placer);
  void CheckTargetDensity() const;
  void CheckNets();
  void SanityCheck();
  void UpdateMovableBlkPlacementStatus();

  /****File I/O member functions****/
  virtual void GenMATLABWellTable(
      std::string const &name_of_file,
      [[maybe_unused]]int32_t well_emit_mode
  ) {
    ckt_ptr_->GenMATLABWellTable(name_of_file);
  }
  void GenMATLABScriptPlaced(std::string const &name_of_file = "block_net_list.m");
  bool SaveNodeTerminal(
      std::string const &terminal_file = "terminal.txt",
      std::string const &node_file = "nodes.txt"
  );
  virtual void EmitDEFWellFile(
      [[maybe_unused]]std::string const &name_of_file,
      [[maybe_unused]]int32_t well_emit_mode,
      [[maybe_unused]]bool enable_emitting_cluster = true
  );

  /****for testing purposes****/
  void ShiftX(double shift_x);
  void ShiftY(double shift_y);

  bool IsDummyBlock(Block &blk);
 protected:
  /* essential data entries */
  int32_t num_threads_;
  double aspect_ratio_; // placement region Height/Width
  double placement_density_;

  /* the following entries are derived data
   * note that the following entries can be manually changed
   * if so, the aspect_ratio_ or filling_rate_ might also be changed */
  int32_t left_, right_, bottom_, top_;
  // boundaries of the placement region
  Circuit *ckt_ptr_;

  // record start/end time
  ElapsedTime elapsed_time_;

  double GetBlkHPWL(Block &blk);

  virtual void PrintStartStatement(std::string const &name_of_process);
  virtual void PrintEndStatement(std::string const &name_of_process, bool is_success);
};

}

#endif //DALI_PLACER_PLACER_H_
