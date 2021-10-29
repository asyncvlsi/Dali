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
#ifndef DALI_DALI_PLACER_PLACER_H_
#define DALI_DALI_PLACER_PLACER_H_

#include <fstream>
#include <iostream>
#include <string>

#include <common/config.h>

#include "dali/circuit/circuit.h"
#include "dali/common/timing.h"

namespace dali {

class Placer {
 protected:
  /* essential data entries */
  double aspect_ratio_; // placement region Height/Width
  double filling_rate_;

  /* the following entries are derived data
   * note that the following entries can be manually changed
   * if so, the aspect_ratio_ or filling_rate_ might also be changed */
  int left_, right_, bottom_, top_;
  // boundaries of the placement region
  Circuit *p_ckt_;

  double GetBlkHPWL(Block &blk);

 public:
  Placer();
  Placer(double aspect_ratio, double filling_rate);
  virtual ~Placer() = default;

  virtual void LoadConf(std::string const &config_file) {
    BOOST_LOG_TRIVIAL(warning)
      << "This is a virtual function, which is not supposed to be called directly\n";
  };

  virtual void SetInputCircuit(Circuit *circuit) {
    DaliExpects(circuit != nullptr,
                "Invalid input circuit: not allowed to set nullptr as an input!");
    if (circuit->Blocks().empty()) {
      BOOST_LOG_TRIVIAL(info)
        << "Invalid input circuit: empty block list, nothing to place!\n";
      return;
    }
    if (circuit->Nets().empty()) {
      BOOST_LOG_TRIVIAL(info)
        << "Bad input circuit: empty net list, nothing to optimize during placement! But anyway...\n";
    }
    p_ckt_ = circuit;
  }
  Circuit *GetCircuit() { return p_ckt_; }
  void SetFillingRate(double rate = 2.0 / 3.0) {
    DaliExpects((rate <= 1) && (rate > 0),
                "Invalid value: value should be in range (0, 1]");
    filling_rate_ = rate;
  }
  double FillingRate() const { return filling_rate_; }
  void SetAspectRatio(double ratio = 1.0) {
    DaliExpects(ratio >= 0,
                "Invalid value: value should be in range (0, +infinity)");
    aspect_ratio_ = ratio;
  }
  double AspectRatio() const { return aspect_ratio_; }
  void SetSpaceBlockRatio(double ratio) {
    DaliExpects(ratio >= 1,
                "Invalid value: value should be in range [1, +infinity)");
    filling_rate_ = 1. / ratio;
  }

  std::vector<Block> &Blocks() { return p_ckt_->Blocks(); }
  std::vector<Net> &Nets() { return p_ckt_->Nets(); }

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
    DaliExpects(p_ckt_ != nullptr,
                "No input circuit specified, cannot modify any circuits!");
    GetCircuit()->NetSortBlkPin();
  }
  virtual bool StartPlacement();

  double WeightedHPWLX() {
    DaliExpects(p_ckt_ != nullptr,
                "No input circuit specified, cannot compute WeightedHPWLX!");
    return GetCircuit()->WeightedHPWLX();
  }
  double WeightedHPWLY() {
    DaliExpects(p_ckt_ != nullptr,
                "No input circuit specified, cannot compute WeightedHPWLY!");
    return GetCircuit()->WeightedHPWLY();
  }
  double WeightedHPWL() {
    DaliExpects(p_ckt_ != nullptr,
                "No input circuit specified, cannot compute HPWL!");
    return GetCircuit()->WeightedHPWL();
  }

  void ReportHPWL() {
    DaliExpects(p_ckt_ != nullptr,
                "No input circuit specified, cannot compute HPWL!");
    p_ckt_->ReportHPWL();
  }

  void ReportHPWLCtoC() {
    DaliExpects(p_ckt_ != nullptr,
                "No input circuit specified, cannot compute HPWLCtoC!");
    GetCircuit()->ReportHPWLCtoC();
  }

  void TakeOver(Placer *placer);
  void SanityCheck();
  void UpdateMovableBlkPlacementStatus();

  /****File I/O member functions****/
  void GenMATLABTable(std::string const &name_of_file) {
    p_ckt_->GenMATLABTable(name_of_file);
  }
  virtual void GenMATLABWellTable(
      std::string const &name_of_file,
      int well_emit_mode
  ) {
    p_ckt_->GenMATLABWellTable(name_of_file);
  }
  void GenMATLABScriptPlaced(std::string const &name_of_file = "block_net_list.m");
  bool SaveNodeTerminal(
      std::string const &terminal_file = "terminal.txt",
      std::string const &node_file = "nodes.txt"
                                     "");
  void SaveDEFFile(std::string const &name_of_file = "circuit.def");
  void SaveDEFFile(
      std::string const &name_of_file,
      std::string const &input_def_file
  );
  virtual void EmitDEFWellFile(
      std::string const &name_of_file,
      int well_emit_mode,
      bool enable_emitting_cluster = true
  );

  /****for testing purposes****/
  void ShiftX(double shift_x);
  void ShiftY(double shift_y);
};

}

#endif //DALI_DALI_PLACER_PLACER_H_
