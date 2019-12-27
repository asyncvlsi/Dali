//
// Created by Yihang Yang on 2019-05-23.
//

#include <cmath>
#include "../common/misc.h"
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
  std::vector<Net> &net_list = GetCircuit()->net_list;
  for (auto &&idx: blk.net_list) {
    hpwl += net_list[idx].HPWL();
  }
  return hpwl;
}

bool Placer::IsBoundaryProper() {
  if (circuit_->MaxWidth() > Right() - Left()) {
    std::cout << "Improper boundary: maximum cell width is larger than the width of placement region\n";
    return false;
  }
  if (circuit_->MaxHeight() > Top() - Bottom()) {
    std::cout << "Improper boundary: maximum cell height is larger than the height of placement region\n";
    return false;
  }

  return true;
}

void Placer::SetBoundaryAuto() {
  Assert(circuit_ != nullptr, "Must set input circuit before setting boundaries");
  long int tot_block_area = circuit_->TotArea();
  int width = std::ceil(std::sqrt(tot_block_area/aspect_ratio_/filling_rate_));
  int height = std::ceil(width * aspect_ratio_);
  std::cout << "Pre-set aspect ratio: " << aspect_ratio_ << "\n";
  aspect_ratio_ = height/(double)width;
  std::cout << "Adjusted aspect rate: " << aspect_ratio_ << "\n";
  left_ = (int)(circuit_->AveWidth());
  right_ = left_ + width;
  bottom_ = (int)(circuit_->AveWidth());
  top_ = bottom_ + height;
  int area = height * width;
  std::cout << "Pre-set filling rate: " << filling_rate_ << "\n";
  filling_rate_ = tot_block_area/(double)area;
  std::cout << "Adjusted filling rate: " << filling_rate_ << "\n";
  Assert(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::SetBoundary(int left, int right, int bottom, int top) {
  Assert(circuit_ != nullptr, "Must set input circuit before setting boundaries");
  Assert(left < right, "Invalid boundary setting: left boundary should be less than right boundary!");
  Assert(bottom < top, "Invalid boundary setting: bottom boundary should be less than top boundary!");
  long int tot_block_area = circuit_->TotArea();
  long int tot_area = (right - left) * (top - bottom);
  Assert(tot_area >= tot_block_area, "Invalid boundary setting: given region has smaller area than total block area!");
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "Pre-set filling rate: " << filling_rate_ << "\n";
  }
  filling_rate_ = (double)tot_block_area/(double)tot_area;
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
  left_ = GetCircuit()->def_left;
  right_ = GetCircuit()->def_right;
  bottom_ = GetCircuit()->def_bottom;
  top_ = GetCircuit()->def_top;
  Assert(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::ReportBoundaries() {
  if (globalVerboseLevel >= LOG_DEBUG) {
    std::cout << "Left, Right, Bottom, Top:\n";
    std::cout << "  " << Left() << ", " << Right() << ", " << Bottom() << ", " << Top() << "\n";
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
  aspect_ratio_ = (top_ - bottom_)/(double)(right_ - left_);
}

void Placer::TakeOver(Placer *placer) {
  aspect_ratio_ = placer->AspectRatio();
  filling_rate_ = placer->FillingRate();
  left_ = placer->Left();
  right_ = placer->Right();
  bottom_ = placer->Bottom();
  top_ = placer->Top();
  circuit_ = placer->GetCircuit();
}

void Placer::GenMATLABScript(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << Left() << " " << Bottom() << " " << Right() - Left() << " " << Top() - Bottom() << "\n";
  for (auto &&block: circuit_->block_list) {
    ost << block.LLX() << " " << block.LLY() << " " << block.Width() << " " << block.Height() << "\n";
  }
  ost.close();
}

void Placer::GenMATLABTable(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << Left() << "\t" << Right() << "\t" << Right() << "\t" << Left() << "\t" << Bottom() << "\t" << Bottom() << "\t" << Top() << "\t" << Top() << "\n";
  for (auto &block: circuit_->block_list) {
    ost << block.LLX() << "\t"
        << block.URX() << "\t"
        << block.URX() << "\t"
        << block.LLX() << "\t"
        << block.LLY() << "\t"
        << block.LLY() << "\t"
        << block.URY() << "\t"
        << block.URY() << "\n";
  }

}

void Placer::GenMATLABWellTable(std::string const &name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  std::string unplug_file = name_of_file + "_unplug.txt";
  std::string plug_file = name_of_file + "_plug.txt";
  GenMATLABTable(frame_file);

  std::ofstream ost(unplug_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + unplug_file);
  for (auto &block: circuit_->block_list) {
    auto well = block.Type()->GetWell();
    if (well != nullptr) {
      if (!well->IsPlug()) {
        auto n_well_shape = well->GetNWellShape();
        ost << block.LLX() + n_well_shape->LLX() << "\t"
            << block.LLX() + n_well_shape->URX() << "\t"
            << block.LLX() + n_well_shape->URX() << "\t"
            << block.LLX() + n_well_shape->LLX() << "\t"
            << block.LLY() + n_well_shape->LLY() << "\t"
            << block.LLY() + n_well_shape->LLY() << "\t"
            << block.LLY() + n_well_shape->URY() << "\t"
            << block.LLY() + n_well_shape->URY() << "\n";
      }
    }
  }

  ost.close();
}

void Placer::GenMATLABScriptPlaced(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << Left() << " " << Bottom() << " " << Right() - Left() << " " << Top() - Bottom() << "\n";
  for (auto &&block: circuit_->block_list) {
    if (block.is_placed) {
      ost << block.LLX() << " " << block.LLY() << " " << block.Width() << " " << block.Height() << "\n";
    }
  }
  ost.close();
}

bool Placer::SaveNodeTerminal(std::string const &terminal_file, std::string const &node_file) {
  std::ofstream ost(terminal_file.c_str());
  std::ofstream ost1(node_file.c_str());
  Assert(ost.is_open() && ost1.is_open(), "Cannot open file " + terminal_file + " or " + node_file);
  for (auto &&block: circuit_->block_list) {
    if (block.IsMovable()) {
      ost1 << block.X() << "\t" << block.Y() << "\n";
    }
    else {
      double low_x, low_y, width, height;
      width = block.Width();
      height = block.Height();
      low_x = block.LLX();
      low_y = block.LLY();
      for (int j=0; j<height; j++) {
        ost << low_x << "\t" << low_y+j << "\n";
        ost << low_x+width << "\t" << low_y+j << "\n";
      }
      for (int j=0; j<width; j++) {
        ost << low_x+j << "\t" << low_y << "\n";
        ost << low_x+j << "\t" << low_y+height << "\n";
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
  std::cout << circuit_->block_list.size() << "\n";
  ost << "COMPONENTS " << circuit_->block_list.size() << " ;\n";
  for (auto &&block: circuit_->block_list) {
    ost << "- "
        << *(block.Name()) << " "
        << *(block.Type()->Name()) << " + "
        << "PLACED"
        << " ("
        << " " + std::to_string((int)(block.LLX()*circuit_->def_distance_microns* circuit_->GetGridValueX()))
        << " " + std::to_string((int)(block.LLY()*circuit_->def_distance_microns* circuit_->GetGridValueY()))
        << " ) "
        << OrientStr(block.Orient()) + " ;\n";
  }
  ost << "END COMPONENTS\n";

  // 3. print net
  ost << "NETS " << circuit_->net_list.size() << " ;\n";
  for (auto &&net: circuit_->net_list) {
    ost << "- "
        << *(net.Name()) << "\n";
    ost << " ";
    for (auto &&pin_pair: net.blk_pin_list) {
      ost << " ( " << *(pin_pair.BlockName()) << " " << *(pin_pair.PinName()) << " ) ";
    }
    ost << "\n" << " ;\n";
  }
  ost << "END NETS\n\n";
  ost << "END DESIGN\n";
}

void Placer::SaveDEFFile(std::string const &name_of_file, std::string const &input_def_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  std::ifstream ist(input_def_file.c_str());
  Assert(ist.is_open(), "Cannot open output file: " + input_def_file);

  std::string line;
  // 1. print file header, copy from def file
  while (line.find("COMPONENTS") == std::string::npos && !ist.eof()) {
    getline(ist,line);
    ost << line << "\n";
  }

  // 2. print component
  //std::cout << _circuit->block_list.size() << "\n";
  for (auto &&block: circuit_->block_list) {
    ost << "- "
        << *(block.Name()) << " "
        << *(block.Type()->Name()) << " + "
        << "PLACED"
        << " ("
        << " " + std::to_string((int)(block.LLX()*circuit_->def_distance_microns* circuit_->GetGridValueX()))
        << " " + std::to_string((int)(block.LLY()*circuit_->def_distance_microns* circuit_->GetGridValueY()))
        << " ) "
        << OrientStr(block.Orient()) + " ;\n";
  }
  ost << "END COMPONENTS\n";
  // jump to the end of components
  while (line.find("END COMPONENTS") == std::string::npos && !ist.eof()) {
    getline(ist,line);
  }

  // 3. print net, copy from def file
  while (!ist.eof()) {
    getline(ist,line);
    ost << line << "\n";
  }
  /*
  ost << "NETS " << net_list.size() << " ;\n";
  for (auto &&net: net_list) {
    ost << "- "
        << net.name() << "\n";
    ost << " ";
    for (auto &&pin: net.pin_list) {
      ost << " " << pin.pin_name();
    }
    ost << "\n" << " ;\n";
  }
  ost << "END NETS\n\n";
  ost << "END DESIGN\n";
   */

  ost.close();
  ist.close();
}

void Placer::SanityCheck() {
  double epsilon = 1e-3;
  Assert(filling_rate_ > epsilon, "Filling rate should be in a proper range, for example [0.1, 1], current value: " + std::to_string(filling_rate_));
  for (auto &&net: GetCircuit()->net_list) {
    Assert(!net.blk_pin_list.empty(), "Empty net?" + *net.Name());
  }
}

void Placer::UpdateComponentsPlacementStatus() {
  for (auto &&blk: circuit_->block_list) {
    blk.SetPlaceStatus(PLACED);
  }
}

void Placer::IOPinPlacement() {
  if (circuit_->pin_list.empty()) return;
  Net *net = nullptr;
  double net_left, net_right, net_bottom, net_top;
  double to_left, to_right, to_bottom, to_top;
  double min_distance;
  for (auto &&iopin: circuit_->pin_list) {
    net = iopin.GetNet();

    net->UpdateMaxMin();
    net_left = net->Left();
    net_right = net->Right();
    net_bottom = net->Bottom();
    net_top = net->Top();

    to_left = net_left - Left();
    to_right = Right() - net_right;
    to_bottom = net_bottom - Bottom();
    to_top = Top() - net_top;
    min_distance = std::min(std::min(to_left, to_right), std::min(to_bottom, to_top));

    if (std::fabs(min_distance-to_left)<1e-10) {
      iopin.SetLoc(Left(), (net_top+net_bottom)/2);
    } else if (std::fabs(min_distance-to_right)<1e-10) {
      iopin.SetLoc(Right(), (net_top+net_bottom)/2);
    } else if (std::fabs(min_distance-to_bottom)<1e-10) {
      iopin.SetLoc((net_left+net_right)/2,Bottom());
    } else {
      iopin.SetLoc((net_left+net_right)/2,Top());
    }
  }
}
