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
#include "placer.h"

#include <cmath>

#include <algorithm>

#include "dali/common/misc.h"

namespace dali {

Placer::Placer() {
  aspect_ratio_ = 0;
  placement_density_ = 0;
  left_ = 0;
  right_ = 0;
  bottom_ = 0;
  top_ = 0;
  p_ckt_ = nullptr;
}

Placer::Placer(double aspect_ratio, double filling_rate) : aspect_ratio_(
    aspect_ratio), placement_density_(filling_rate) {
  left_ = 0;
  right_ = 0;
  bottom_ = 0;
  top_ = 0;
  p_ckt_ = nullptr;
}

void Placer::LoadConf(std::string const &config_file) {
  BOOST_LOG_TRIVIAL(warning)
    << "This is a virtual function, which is not supposed to be called directly\n";
};

void Placer::SetInputCircuit(Circuit *circuit) {
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

Circuit &Placer::GetCkt() {
  DaliExpects(p_ckt_ != nullptr,
              "Please set an input circuit before using this method");
  return *p_ckt_;
}

double Placer::GetBlkHPWL(Block &blk) {
  double hpwl = 0;
  std::vector<Net> &net_list = Nets();
  for (auto &idx : blk.NetList()) {
    hpwl += net_list[idx].WeightedHPWL();
  }
  return hpwl;
}

bool Placer::IsBoundaryProper() {
  if (p_ckt_->MaxBlkWidth() > RegionRight() - RegionLeft()) {
    BOOST_LOG_TRIVIAL(info)
      << "Problematic placement boundary:\n"
      << "    maximum cell width is larger than the width of placement region\n";
    return false;
  }
  if (p_ckt_->MaxBlkHeight() > RegionTop() - RegionBottom()) {
    BOOST_LOG_TRIVIAL(info)
      << "Problematic placement boundary:\n"
      << "    maximum cell height is larger than the height of placement region\n";
    return false;
  }
  return true;
}

void Placer::SetBoundaryAuto() {
  DaliExpects(p_ckt_ != nullptr,
              "Must set input circuit before setting boundaries");
  long int tot_block_area = p_ckt_->TotBlkArea();
  int width = std::ceil(std::sqrt(
      double(tot_block_area) / aspect_ratio_ / placement_density_));
  int height = std::ceil(width * aspect_ratio_);
  BOOST_LOG_TRIVIAL(info) << "Pre-set aspect ratio: " << aspect_ratio_
                          << "\n";
  aspect_ratio_ = height / (double) width;
  BOOST_LOG_TRIVIAL(info) << "Adjusted aspect rate: " << aspect_ratio_
                          << "\n";
  left_ = (int) (p_ckt_->AveBlkWidth());
  right_ = left_ + width;
  bottom_ = (int) (p_ckt_->AveBlkWidth());
  top_ = bottom_ + height;
  int area = height * width;
  BOOST_LOG_TRIVIAL(info) << "Pre-set filling rate: " << placement_density_
                          << "\n";
  placement_density_ = double(tot_block_area) / area;
  BOOST_LOG_TRIVIAL(info) << "Adjusted filling rate: " << placement_density_
                          << "\n";
  DaliExpects(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::SetBoundary(int left, int right, int bottom, int top) {
  DaliExpects(p_ckt_ != nullptr,
              "Must set input circuit before setting boundaries");
  DaliExpects(left < right,
              "Invalid boundary setting: left boundary should be less than right boundary!");
  DaliExpects(bottom < top,
              "Invalid boundary setting: bottom boundary should be less than top boundary!");
  unsigned long long tot_block_area = p_ckt_->TotBlkArea();
  unsigned long long tot_area = (unsigned long long) (right - left) *
      (unsigned long long) (top - bottom);
  DaliExpects(tot_area >= tot_block_area,
              "Invalid boundary setting: given region has smaller area than total block area!");
  BOOST_LOG_TRIVIAL(info) << "Pre-set filling rate: " << placement_density_
                          << "\n";
  placement_density_ = (double) tot_block_area / (double) tot_area;
  BOOST_LOG_TRIVIAL(info) << "Adjusted filling rate: " << placement_density_
                          << "\n";
  left_ = left;
  right_ = right;
  bottom_ = bottom;
  top_ = top;
  DaliExpects(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::SetBoundaryDef() {
  left_ = GetCkt().RegionLLX();
  right_ = GetCkt().RegionURX();
  bottom_ = GetCkt().RegionLLY();
  top_ = GetCkt().RegionURY();
  DaliExpects(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::ReportBoundaries() const {
  BOOST_LOG_TRIVIAL(info)
    << "Left, Right, Bottom, Top:\n  "
    << RegionLeft() << ", "
    << RegionRight() << ", "
    << RegionBottom() << ", "
    << RegionTop() << "\n";
}

void Placer::UpdateAspectRatio() {
  if ((right_ - left_ == 0) || (top_ - bottom_ == 0)) {
    BOOST_LOG_TRIVIAL(fatal) << "Error!\n"
                             << "Zero Height or Width of placement region!\n";
    ReportBoundaries();
    exit(1);
  }
  aspect_ratio_ = (top_ - bottom_) / (double) (right_ - left_);
}

bool Placer::StartPlacement() {
  BOOST_LOG_TRIVIAL(fatal)
    << "Error!\n"
    << "This function should not be called! You need to implement it yourself!\n";
  return false;
}

void Placer::TakeOver(Placer *placer) {
  aspect_ratio_ = placer->AspectRatio();
  placement_density_ = placer->PlacementDensity();
  left_ = placer->RegionLeft();
  right_ = placer->RegionRight();
  bottom_ = placer->RegionBottom();
  top_ = placer->RegionTop();
  p_ckt_ = placer->p_ckt_;
}

void Placer::GenMATLABScriptPlaced(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " << name_of_file);
  ost << RegionLeft() << " " << RegionBottom() << " "
      << RegionRight() - RegionLeft() << " "
      << RegionTop() - RegionBottom() << "\n";
  for (auto &block : Blocks()) {
    if (block.IsPlaced()) {
      ost << block.LLX() << " " << block.LLY() << " " << block.Width()
          << " " << block.Height() << "\n";
    }
  }
  ost.close();
}

bool Placer::SaveNodeTerminal(
    std::string const &terminal_file,
    std::string const &node_file
) {
  std::ofstream ost(terminal_file.c_str());
  std::ofstream ost1(node_file.c_str());
  DaliExpects(ost.is_open() && ost1.is_open(),
              "Cannot open file " << terminal_file << " or " << node_file);
  for (auto &block : Blocks()) {
    if (block.IsMovable()) {
      ost1 << block.X() << "\t" << block.Y() << "\n";
    } else {
      double low_x, low_y, width, height;
      width = block.Width();
      height = block.Height();
      low_x = block.LLX();
      low_y = block.LLY();
      for (int j = 0; j < height; j++) {
        ost << low_x << "\t" << low_y + j << "\n";
        ost << low_x + width << "\t" << low_y + j << "\n";
      }
      for (int j = 0; j < width; j++) {
        ost << low_x + j << "\t" << low_y << "\n";
        ost << low_x + j << "\t" << low_y + height << "\n";
      }
    }
  }
  ost.close();
  ost1.close();
  return true;
}

void Placer::EmitDEFWellFile(
    std::string const &name_of_file,
    int well_emit_mode,
    bool enable_emitting_cluster
) {
  BOOST_LOG_TRIVIAL(info)
    << "virtual function Placer::EmitDEFWellFile() does nothing, you should not use this member function\n";
}

void Placer::SanityCheck() {
  double epsilon = 1e-3;
  BOOST_LOG_TRIVIAL(info) << "Target density: " << placement_density_ << "\n";
  DaliExpects(placement_density_ > epsilon,
              "Filling rate should be in a proper range, for example [0.1, 1], current value: "
                  << placement_density_);
  for (auto &net : Nets()) {
    if (net.BlockPins().empty()) {
      DaliWarns(true,
                "Empty net or this net only contains unplaced IOPINs: "
                    << net.Name());
    }
  }
  DaliExpects(IsBoundaryProper(), "Improper boundary setting");
}

void Placer::UpdateMovableBlkPlacementStatus() {
  for (auto &blk : Blocks()) {
    if (blk.IsMovable()) {
      blk.SetPlacementStatus(PLACED);
    }
  }
}

void Placer::ShiftX(double shift_x) {
  for (auto &block : Blocks()) {
    block.IncreaseX(shift_x);
  }
}

void Placer::ShiftY(double shift_y) {
  for (auto &block : Blocks()) {
    block.IncreaseY(shift_y);
  }
}

bool Placer::IsDummyBlock(Block &blk) {
  return blk.TypePtr() == p_ckt_->tech().IoDummyBlkTypePtr();
}

}
