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

#include <omp.h>

#include <algorithm>
#include <cmath>

#include "dali/common/misc.h"

namespace dali {

Placer::Placer() :
    num_threads_(1),
    aspect_ratio_(0),
    placement_density_(0),
    left_(0),
    right_(0),
    bottom_(0),
    top_(0),
    ckt_ptr_(nullptr) {}

Placer::Placer(double aspect_ratio, double filling_rate) :
    num_threads_(1),
    aspect_ratio_(aspect_ratio),
    placement_density_(filling_rate),
    left_(0),
    right_(0),
    bottom_(0),
    top_(0),
    ckt_ptr_(nullptr) {}

void Placer::LoadConf([[maybe_unused]]std::string const &config_file) {
  BOOST_LOG_TRIVIAL(warning)
    << "This is a virtual function, which is not supposed to be called directly\n";
};

void Placer::SetInputCircuit(Circuit *circuit) {
  DaliExpects(
      circuit != nullptr,
      "Invalid input circuit: not allowed to set nullptr as an input!"
  );
  ckt_ptr_ = circuit;
  if (ckt_ptr_->Blocks().empty()) {
    BOOST_LOG_TRIVIAL(info) << "Empty block list, nothing to place!\n";
  }
  if (ckt_ptr_->Nets().empty()) {
    BOOST_LOG_TRIVIAL(info) << "Empty net list, nothing to optimize!\n";
  }
  SetBoundaryFromCircuit();
}

/****
 * @brief Set the number of threads which can be used by this placer
 *
 * @param num_threads: the number of threads
 */
void Placer::SetNumThreads(int num_threads) {
  if (num_threads < 1) {
    num_threads = 1;
    DaliWarning(
        "Number of threads ("
            << num_threads << ") for placer is less than 1, using 1 instead"
    );
  }
  int max_num_threads = omp_get_max_threads();
  if (num_threads > max_num_threads) {
    num_threads = max_num_threads;
    DaliWarning(
        "Number of threads ("
            << num_threads
            << ") for placer is more than max number of threads ("
            << max_num_threads << "), using max number of threads instead"
    );
  }
  num_threads_ = num_threads;
}

void Placer::SetPlacementDensity(double density) {
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

double Placer::PlacementDensity() const {
  return placement_density_;
}

void Placer::SetAspectRatio(double ratio) {
  DaliExpects(
      ratio >= 0,
      "Invalid value: value should be in range (0, +infinity)"
  );
  aspect_ratio_ = ratio;
}

double Placer::AspectRatio() const {
  return aspect_ratio_;
}

void Placer::SetSpaceBlockRatio(double ratio) {
  DaliExpects(
      ratio >= 1,
      "Invalid value: value should be in range [1, +infinity)"
  );
  placement_density_ = 1.0 / ratio;
}

double Placer::GetBlkHPWL(Block &blk) {
  double hpwl = 0;
  std::vector<Net> &nets = ckt_ptr_->Nets();
  for (auto &idx : blk.NetList()) {
    hpwl += nets[idx].WeightedHPWL();
  }
  return hpwl;
}

void Placer::CheckPlacementBoundary() {
  DaliExpects(
      ckt_ptr_->MaxBlkWidth() <= RegionRight() - RegionLeft(),
      "maximum cell width is larger than the width of placement region"
  );
  DaliExpects(
      ckt_ptr_->MaxBlkHeight() <= RegionTop() - RegionBottom(),
      "maximum cell height is larger than the height of placement region"
  );
}

void Placer::SetBoundaryAuto() {
  DaliExpects(ckt_ptr_ != nullptr,
              "Must set input circuit before setting boundaries");
  auto tot_block_area = ckt_ptr_->TotBlkArea();
  int width = std::ceil(std::sqrt(
      double(tot_block_area) / aspect_ratio_ / placement_density_));
  int height = std::ceil(width * aspect_ratio_);
  BOOST_LOG_TRIVIAL(info) << "Pre-set aspect ratio: " << aspect_ratio_
                          << "\n";
  aspect_ratio_ = height / (double) width;
  BOOST_LOG_TRIVIAL(info) << "Adjusted aspect rate: " << aspect_ratio_
                          << "\n";
  left_ = (int) (ckt_ptr_->AveBlkWidth());
  right_ = left_ + width;
  bottom_ = (int) (ckt_ptr_->AveBlkWidth());
  top_ = bottom_ + height;
  int area = height * width;
  BOOST_LOG_TRIVIAL(info) << "Pre-set filling rate: " << placement_density_
                          << "\n";
  placement_density_ = double(tot_block_area) / area;
  BOOST_LOG_TRIVIAL(info) << "Adjusted filling rate: " << placement_density_
                          << "\n";
  CheckPlacementBoundary();
}

void Placer::SetBoundary(int left, int right, int bottom, int top) {
  DaliExpects(ckt_ptr_ != nullptr,
              "Must set input circuit before setting boundaries");
  DaliExpects(left < right,
              "Invalid boundary setting: left boundary should be less than right boundary!");
  DaliExpects(bottom < top,
              "Invalid boundary setting: bottom boundary should be less than top boundary!");
  unsigned long long tot_block_area = ckt_ptr_->TotBlkArea();
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
  CheckPlacementBoundary();
}

void Placer::SetBoundaryFromCircuit() {
  left_ = ckt_ptr_->RegionLLX();
  right_ = ckt_ptr_->RegionURX();
  bottom_ = ckt_ptr_->RegionLLY();
  top_ = ckt_ptr_->RegionURY();
  CheckPlacementBoundary();
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
  DaliExpects(right_ > left_, "Right boundary is less than left boundary?");
  DaliExpects(top_ > bottom_, "Top boundary is less than bottom boundary?");
  aspect_ratio_ = (top_ - bottom_) / (double) (right_ - left_);
}

void Placer::NetSortBlkPin() {
  DaliExpects(
      ckt_ptr_ != nullptr,
      "No input circuit specified, cannot modify any circuits!"
  );
  ckt_ptr_->NetSortBlkPin();
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
  ckt_ptr_ = placer->ckt_ptr_;
}

void Placer::GenMATLABScriptPlaced(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " << name_of_file);
  ost << RegionLeft() << " " << RegionBottom() << " "
      << RegionRight() - RegionLeft() << " "
      << RegionTop() - RegionBottom() << "\n";
  auto &blocks = ckt_ptr_->Blocks();
  for (auto &block : blocks) {
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
  auto &blocks = ckt_ptr_->Blocks();
  for (auto &block : blocks) {
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
    [[maybe_unused]]std::string const &name_of_file,
    [[maybe_unused]]int well_emit_mode,
    [[maybe_unused]]bool enable_emitting_cluster
) {
  DaliFatal("You should not use this member function");
}

/****
 * @brief: check if the target density if properly set
 */
void Placer::CheckTargetDensity() const {
  double epsilon = 1e-3;
  BOOST_LOG_TRIVIAL(info) << "  target density: " << placement_density_ << "\n";
  DaliExpects(
      placement_density_ > epsilon,
      "Filling rate should be in a proper range, for example [0.1, 1], current value: "
          << placement_density_
  );
}

/****
 * @brief: check if there is any empty nets
 */
void Placer::CheckNets() { // TODO: empty nets should be allowed
  auto &nets = ckt_ptr_->Nets();
  for (auto &net : nets) {
    if (net.BlockPins().empty()) {
      DaliWarning(
          "Empty net or this net only contains unplaced IOPINs: " << net.Name()
      );
    }
  }
}

/****
 * @brief: perform basic sanity check
 */
void Placer::SanityCheck() {
  CheckTargetDensity();
  CheckNets();
  CheckPlacementBoundary();
}

void Placer::UpdateMovableBlkPlacementStatus() {
  auto &blocks = ckt_ptr_->Blocks();
  for (auto &block : blocks) {
    if (block.IsMovable()) {
      block.SetPlacementStatus(PLACED);
    }
  }
}

void Placer::ShiftX(double shift_x) {
  auto &blocks = ckt_ptr_->Blocks();
  for (auto &block : blocks) {
    block.IncreaseX(shift_x);
  }
}

void Placer::ShiftY(double shift_y) {
  auto &blocks = ckt_ptr_->Blocks();
  for (auto &block : blocks) {
    block.IncreaseY(shift_y);
  }
}

bool Placer::IsDummyBlock(Block &blk) {
  return blk.TypePtr() == ckt_ptr_->tech().IoDummyBlkTypePtr();
}

void Placer::PrintStartStatement(std::string const &name_of_process) {
  elapsed_time_.RecordStartTime();
  PrintHorizontalLine();
  BOOST_LOG_TRIVIAL(info) << "Start " << name_of_process << "\n";
}

void Placer::PrintEndStatement(
    std::string const &name_of_process,
    bool is_success
) {
  ReportHPWL();

  if (is_success) {
    BOOST_LOG_TRIVIAL(info)
      << "\033[0;36m" << name_of_process << " completed" << "\033[0m\n";
  } else {
    BOOST_LOG_TRIVIAL(info)
      << "\033[0;31m" << name_of_process << " failed" << "\033[0m\n";
  }

  // report time
  elapsed_time_.RecordEndTime();
  elapsed_time_.PrintTimeElapsed();

  // report memory
  ReportMemory();

}

}
