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
#include "dali/common/timing.h"

namespace dali {

class Placer {
 public:
  Placer();
  Placer(double aspect_ratio, double filling_rate);
  virtual ~Placer() = default;

  virtual void LoadConf([[maybe_unused]]std::string const &config_file);

  virtual void SetInputCircuit(Circuit *circuit);

  void SetPlacementDensity(double density = 2.0 / 3.0) {
    DaliExpects(
        (density <= 1) && (density > 0),
        "Invalid value: value should be in range (0, 1]"
    );
    DaliExpects(
        ckt_ptr_->WhiteSpaceUsage() < density,
        "Cannot set target density smaller than average white space utility!"
    );
    placement_density_ = density;
  }
  double PlacementDensity() const { return placement_density_; }
  void SetAspectRatio(double ratio = 1.0) {
    DaliExpects(
        ratio >= 0,
        "Invalid value: value should be in range (0, +infinity)"
    );
    aspect_ratio_ = ratio;
  }
  double AspectRatio() const { return aspect_ratio_; }
  void SetSpaceBlockRatio(double ratio) {
    DaliExpects(
        ratio >= 1,
        "Invalid value: value should be in range [1, +infinity)"
    );
    placement_density_ = 1.0 / ratio;
  }

  bool IsBoundaryProper();
  void SetBoundaryAuto();
  void SetBoundary(int left, int right, int bottom, int top);
  void SetBoundaryDef();
  void ReportBoundaries() const;

  int RegionLeft() const { return left_; }
  int RegionRight() const { return right_; }
  int RegionBottom() const { return bottom_; }
  int RegionTop() const { return top_; }
  int RegionWidth() const { return right_ - left_; }
  int RegionHeight() const { return top_ - bottom_; }

  void UpdateAspectRatio();
  void NetSortBlkPin() {
    DaliExpects(
        ckt_ptr_ != nullptr,
        "No input circuit specified, cannot modify any circuits!"
    );
    ckt_ptr_->NetSortBlkPin();
  }
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
  void SanityCheck();
  void UpdateMovableBlkPlacementStatus();

  /****File I/O member functions****/
  void GenMATLABTable(std::string const &name_of_file) {
    ckt_ptr_->GenMATLABTable(name_of_file);
  }
  virtual void GenMATLABWellTable(
      std::string const &name_of_file,
      [[maybe_unused]]int well_emit_mode
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
      [[maybe_unused]]int well_emit_mode,
      [[maybe_unused]]bool enable_emitting_cluster = true
  );

  /****for testing purposes****/
  void ShiftX(double shift_x);
  void ShiftY(double shift_y);

  bool IsDummyBlock(Block &blk);
 protected:
  /* essential data entries */
  double aspect_ratio_; // placement region Height/Width
  double placement_density_;

  /* the following entries are derived data
   * note that the following entries can be manually changed
   * if so, the aspect_ratio_ or filling_rate_ might also be changed */
  int left_, right_, bottom_, top_;
  // boundaries of the placement region
  Circuit *ckt_ptr_;

  double GetBlkHPWL(Block &blk);
};

}

#endif //DALI_PLACER_PLACER_H_
