//
// Created by Yihang Yang on 2019-05-23.
//

#include <cmath>

#include "common/misc.h"
#include "placer.h"

Placer::Placer() {
  aspect_ratio_ = 0;
  filling_rate_ = 0;
  left_ = 0;
  right_ = 0;
  bottom_ = 0;
  top_ = 0;
  circuit_ = nullptr;
}

Placer::Placer(double aspect_ratio, double filling_rate) : aspect_ratio_(aspect_ratio), filling_rate_(filling_rate) {
  left_ = 0;
  right_ = 0;
  bottom_ = 0;
  top_ = 0;
  circuit_ = nullptr;
}

Placer::~Placer() = default;

double Placer::GetBlkHPWL(Block &blk) {
  double hpwl = 0;
  std::vector<Net> &net_list = *(NetList());
  for (auto &idx: blk.net_list) {
    hpwl += net_list[idx].HPWL();
  }
  return hpwl;
}

bool Placer::IsBoundaryProper() {
  if (circuit_->MaxBlkWidth() > RegionRight() - RegionLeft()) {
    std::cout << "Improper boundary:\n"
              << "    maximum cell width is larger than the width of placement region\n";
    return false;
  }
  if (circuit_->MaxBlkHeight() > RegionTop() - RegionBottom()) {
    std::cout << "Improper boundary:\n"
              << "    maximum cell height is larger than the height of placement region\n";
    return false;
  }
  return true;
}

void Placer::SetBoundaryAuto() {
  Assert(circuit_ != nullptr, "Must set input circuit before setting boundaries");
  long int tot_block_area = circuit_->TotBlkArea();
  int width = std::ceil(std::sqrt(double(tot_block_area) / aspect_ratio_ / filling_rate_));
  int height = std::ceil(width * aspect_ratio_);
  std::cout << "Pre-set aspect ratio: " << aspect_ratio_ << "\n";
  aspect_ratio_ = height / (double) width;
  std::cout << "Adjusted aspect rate: " << aspect_ratio_ << "\n";
  left_ = (int) (circuit_->AveBlkWidth());
  right_ = left_ + width;
  bottom_ = (int) (circuit_->AveBlkWidth());
  top_ = bottom_ + height;
  int area = height * width;
  std::cout << "Pre-set filling rate: " << filling_rate_ << "\n";
  filling_rate_ = double(tot_block_area) / area;
  std::cout << "Adjusted filling rate: " << filling_rate_ << "\n";
  Assert(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::SetBoundary(int left, int right, int bottom, int top) {
  Assert(circuit_ != nullptr, "Must set input circuit before setting boundaries");
  Assert(left < right, "Invalid boundary setting: left boundary should be less than right boundary!");
  Assert(bottom < top, "Invalid boundary setting: bottom boundary should be less than top boundary!");
  unsigned long int tot_block_area = circuit_->TotBlkArea();
  unsigned long int tot_area = (right - left) * (top - bottom);
  Assert(tot_area >= tot_block_area, "Invalid boundary setting: given region has smaller area than total block area!");
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "Pre-set filling rate: " << filling_rate_ << "\n";
  }
  filling_rate_ = (double) tot_block_area / (double) tot_area;
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "Adjusted filling rate: " << filling_rate_ << "\n";
  }
  left_ = left;
  right_ = right;
  bottom_ = bottom;
  top_ = top;
  Assert(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::SetBoundaryDef() {
  left_ = GetCircuit()->Left();
  right_ = GetCircuit()->Right();
  bottom_ = GetCircuit()->Bottom();
  top_ = GetCircuit()->Top();
  Assert(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::ReportBoundaries() {
  if (globalVerboseLevel >= LOG_DEBUG) {
    std::cout << "Left, Right, Bottom, Top:\n";
    std::cout << "  " << RegionLeft() << ", " << RegionRight() << ", " << RegionBottom() << ", " << RegionTop() << "\n";
  }
}

void Placer::UpdateAspectRatio() {
  if ((right_ - left_ == 0) || (top_ - bottom_ == 0)) {
    if (globalVerboseLevel >= LOG_ERROR) {
      std::cout << "Error!\n"
                << "Zero Height or Width of placement region!\n";
      ReportBoundaries();
    }
    exit(1);
  }
  aspect_ratio_ = (top_ - bottom_) / (double) (right_ - left_);
}

void Placer::TakeOver(Placer *placer) {
  aspect_ratio_ = placer->AspectRatio();
  filling_rate_ = placer->FillingRate();
  left_ = placer->RegionLeft();
  right_ = placer->RegionRight();
  bottom_ = placer->RegionBottom();
  top_ = placer->RegionTop();
  circuit_ = placer->GetCircuit();
}

void Placer::GenMATLABScriptPlaced(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << RegionLeft() << " " << RegionBottom() << " " << RegionRight() - RegionLeft() << " "
      << RegionTop() - RegionBottom() << "\n";
  for (auto &block: *BlockList()) {
    if (block.IsPlaced()) {
      ost << block.LLX() << " " << block.LLY() << " " << block.Width() << " " << block.Height() << "\n";
    }
  }
  ost.close();
}

bool Placer::SaveNodeTerminal(std::string const &terminal_file, std::string const &node_file) {
  std::ofstream ost(terminal_file.c_str());
  std::ofstream ost1(node_file.c_str());
  Assert(ost.is_open() && ost1.is_open(), "Cannot open file " + terminal_file + " or " + node_file);
  for (auto &block: *BlockList()) {
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

void Placer::SaveDEFFile(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);

  // 1. print file header?
  ost << "VERSION 5.8 ;\n"
      << "DIVIDERCHAR \"/\" ;\n"
      << "BUSBITCHARS \"[]\" ;\n";
  ost << "DESIGN tmp_circuit_name\n";
  ost << "UNITS DISTANCE MICRONS 2000 \n\n"; // temporary values, depends on LEF file

  // no core rows?
  // no tracks?

  // 2. print component
  std::cout << BlockList()->size() << "\n";
  ost << "COMPONENTS " << BlockList()->size() << " ;\n";
  for (auto &block: *BlockList()) {
    ost << "- "
        << *(block.Name()) << " "
        << *(block.Type()->Name()) << " + "
        << "PLACED"
        << " ("
        << " "
            + std::to_string((int) (block.LLX() * circuit_->design_.def_distance_microns * circuit_->GetGridValueX()))
        << " "
            + std::to_string((int) (block.LLY() * circuit_->design_.def_distance_microns * circuit_->GetGridValueY()))
        << " ) "
        << OrientStr(block.Orient()) + " ;\n";
  }
  ost << "END COMPONENTS\n";

  // 3. print net
  ost << "NETS " << NetList()->size() << " ;\n";
  for (auto &net: *NetList()) {
    ost << "- "
        << *(net.Name()) << "\n";
    ost << " ";
    for (auto &pin_pair: net.blk_pin_list) {
      ost << " ( " << *(pin_pair.BlockName()) << " " << *(pin_pair.PinName()) << " ) ";
    }
    ost << "\n" << " ;\n";
  }
  ost << "END NETS\n\n";
  ost << "END DESIGN\n";
}

void Placer::SaveDEFFile(std::string const &name_of_file, std::string const &input_def_file) {
  circuit_->SaveDefFile(name_of_file, input_def_file);
}

void Placer::EmitDEFWellFile(std::string const &name_of_file, std::string const &input_def_file) {
  circuit_->SaveDefFile(name_of_file, input_def_file);
}

void Placer::SanityCheck() {
  double epsilon = 1e-3;
  Assert(filling_rate_ > epsilon,
         "Filling rate should be in a proper range, for example [0.1, 1], current value: "
             + std::to_string(filling_rate_));
  for (auto &net: *NetList()) {
    Assert(!net.blk_pin_list.empty(), "Empty net?" + *net.Name());
  }
  Assert(IsBoundaryProper(), "Improper boundary setting");
}

void Placer::UpdateMovableBlkPlacementStatus() {
  for (auto &blk: *BlockList()) {
    if (blk.IsMovable()) {
      blk.SetPlaceStatus(PLACED);
    }
  }
}

void Placer::IOPinPlacement() {
  if (circuit_->GetIOPinList()->empty()) return;
  Net *net = nullptr;
  double net_minx, net_maxx, net_miny, net_maxy;
  double to_left, to_right, to_bottom, to_top;
  double min_distance;
  for (auto &iopin: *(circuit_->GetIOPinList())) {
    net = iopin.GetNet();

    net->UpdateMaxMinIndex();
    net_minx = net->MinX();
    net_maxx = net->MaxX();
    net_miny = net->MinY();
    net_maxy = net->MaxY();

    to_left = net_minx - left_;
    to_right = right_ - net_maxx;
    to_bottom = net_miny - bottom_;
    to_top = top_ - net_maxy;
    min_distance = std::min(std::min(to_left, to_right), std::min(to_bottom, to_top));

    if (std::fabs(min_distance - to_left) < 1e-10) {
      iopin.SetLoc(left_, (net_maxy + net_miny) / 2);
    } else if (std::fabs(min_distance - to_right) < 1e-10) {
      iopin.SetLoc(right_, (net_maxy + net_miny) / 2);
    } else if (std::fabs(min_distance - to_bottom) < 1e-10) {
      iopin.SetLoc((net_minx + net_maxx) / 2, bottom_);
    } else {
      iopin.SetLoc((net_minx + net_maxx) / 2, top_);
    }
  }
}

void Placer::ShiftX(double shift_x) {
  for (auto &block: *BlockList()) {
    block.IncreaseX(shift_x);
  }
}

void Placer::ShiftY(double shift_y) {
  for (auto &block: *BlockList()) {
    block.IncreaseY(shift_y);
  }
}
