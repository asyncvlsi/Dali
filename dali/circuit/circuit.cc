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
#include "circuit.h"

#include <cfloat>
#include <chrono>
#include <climits>
#include <cmath>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include "dali/common/elapsed_time.h"
#include "dali/common/helper.h"
#include "dali/common/optregdist.h"

#include "dali/circuit/enums.h"

namespace dali {

Circuit::Circuit() {
  AddDummyIOPinBlockType();
}

void Circuit::InitializeFromPhyDB(phydb::PhyDB *phy_db_ptr) {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  DaliExpects(phy_db_ptr != nullptr,
              "Dali cannot initialize from a PhyDB which is a nullptr!");
  phy_db_ptr_ = phy_db_ptr;

  PrintHorizontalLine();
  BOOST_LOG_TRIVIAL(info) << "Load information from PhyDB\n";
  LoadTech(phy_db_ptr_);
  LoadDesign(phy_db_ptr_);
  LoadCell(phy_db_ptr_);
  UpdateTotalBlkArea();

  elapsed_time.RecordEndTime();
  elapsed_time.PrintTimeElapsed();
}

int32_t Circuit::Micron2DatabaseUnit(double x) const {
  return int32_t(std::ceil(x * DatabaseMicrons()));
}

double Circuit::DatabaseUnit2Micron(int32_t x) const {
  return (double(x)) / DatabaseMicrons();
}

int32_t Circuit::DistanceScaleFactorX() const {
  double d_factor_x = GridValueX() * design_.distance_microns_;
  DaliExpects(AbsResidual(d_factor_x, 1.0) < constants_.epsilon,
              "grid_value_x * distance_micron is not close to an integer?");
  return static_cast<int32_t>(std::round(d_factor_x));

}

int32_t Circuit::DistanceScaleFactorY() const {
  double d_factor_y = GridValueY() * design_.distance_microns_;
  DaliExpects(AbsResidual(d_factor_y, 1.0) < constants_.epsilon,
              "grid_value_y * distance_micron is not close to an integer?");
  return static_cast<int32_t>(std::round(d_factor_y));
}

int32_t Circuit::LocDali2PhydbX(double loc) const {
  return static_cast<int32_t>(std::round(loc * DistanceScaleFactorX()))
      + DieAreaOffsetX();
}

int32_t Circuit::LocDali2PhydbY(double loc) const {
  return static_cast<int32_t>(std::round(loc * DistanceScaleFactorY()))
      + DieAreaOffsetY();
}

double Circuit::LocPhydb2DaliX(int32_t loc) const {
  double factor_x = DistanceMicrons() * GridValueX();
  return (loc - DieAreaOffsetX()) / factor_x;
}

double Circuit::LocPhydb2DaliY(int32_t loc) const {
  double factor_y = DistanceMicrons() * GridValueY();
  return (loc - DieAreaOffsetY()) / factor_y;
}

double Circuit::LengthPhydb2DaliX(double length) const {
  int32_t mg_length =
      static_cast<int32_t>(std::round(length / tech_.GetManufacturingGrid()));
  int32_t mg_grid_x = static_cast<int32_t>(
      std::round(GridValueX() / tech_.GetManufacturingGrid())
  );
  return (double) mg_length / (double) mg_grid_x;
}

double Circuit::LengthPhydb2DaliY(double length) const {
  int32_t mg_length =
      static_cast<int32_t>(std::round(length / tech_.GetManufacturingGrid()));
  int32_t mg_grid_y = static_cast<int32_t>(
      std::round(GridValueY() / tech_.GetManufacturingGrid())
  );
  return (double) mg_length / (double) mg_grid_y;
}

Tech &Circuit::tech() {
  return tech_;
}

Design &Circuit::design() {
  return design_;
}

void Circuit::SetDatabaseMicrons(int32_t database_micron) {
  DaliExpects(database_micron > 0,
              "Cannot set negative database microns: Circuit::SetDatabaseMicrons()");
  tech_.database_microns_ = database_micron;
}

int32_t Circuit::DatabaseMicrons() const {
  return tech_.database_microns_;
}

// set manufacturing grid
void Circuit::SetManufacturingGrid(double manufacture_grid) {
  DaliExpects(manufacture_grid > 0,
              "Cannot set negative manufacturing grid: Circuit::setmanufacturing_grid_");
  tech_.manufacturing_grid_ = manufacture_grid;
}

// get manufacturing grid
double Circuit::ManufacturingGrid() const {
  return tech_.manufacturing_grid_;
}

void Circuit::SetGridValue(double grid_value_x, double grid_value_y) {
  DaliExpects(!tech_.is_grid_set_,
              "once set, grid_value cannot be changed!");
  DaliExpects(grid_value_x > 0,
              "grid_value_x must be a positive real number!");
  DaliExpects(grid_value_y > 0,
              "grid_value_y must be a positive real number!");
  double residual_x = AbsResidual(grid_value_x, tech_.GetManufacturingGrid());
  DaliExpects(residual_x < constants_.epsilon,
              "grid value x is not integer multiple of manufacturing grid?");
  double residual_y = AbsResidual(grid_value_y, tech_.GetManufacturingGrid());
  DaliExpects(residual_y < constants_.epsilon,
              "grid value y is not integer multiple of manufacturing grid?");
  tech_.grid_value_x_ = grid_value_x;
  tech_.grid_value_y_ = grid_value_y;
  tech_.is_grid_set_ = true;
}

void Circuit::SetGridFromMetalPitch() {
  DaliExpects(tech_.metal_list_.size() >= 2,
              "No enough metal layer for determining grid value in x and y! Circuit::SetGridFromMetalPitch()");
  MetalLayer *hor_layer = nullptr;
  MetalLayer *ver_layer = nullptr;
  for (auto &metal_layer : tech_.metal_list_) {
    if (hor_layer == nullptr && metal_layer.Direction() == HORIZONTAL) {
      hor_layer = &metal_layer;
    }
    if (ver_layer == nullptr && metal_layer.Direction() == VERTICAL) {
      ver_layer = &metal_layer;
    }
  }
  DaliExpects(
      hor_layer != nullptr,
      "Cannot find a horizontal metal layer! Circuit::SetGridFromMetalPitch()"
  );
  DaliExpects(
      ver_layer != nullptr,
      "Cannot find a vertical metal layer! Circuit::SetGridFromMetalPitch()"
  );
  //BOOST_LOG_TRIVIAL(info)   << "vertical layer: " << *ver_layer->Name() << "  " << ver_layer->PitchX() << "\n";
  //BOOST_LOG_TRIVIAL(info)   << "horizontal layer: " << *hor_layer->Name() << "  " << hor_layer->PitchY() << "\n";
  SetGridValue(ver_layer->PitchX(), hor_layer->PitchY());
}

double Circuit::GridValueX() const {
  return tech_.grid_value_x_;
}

double Circuit::GridValueY() const {
  return tech_.grid_value_y_;
}

void Circuit::SetRowHeight(double row_height) {
  DaliExpects(row_height > 0, "Setting row height to a negative value?");
  //BOOST_LOG_TRIVIAL(info) << row_height << "  " << GridValueY() << std::endl;
  double residual = AbsResidual(row_height, GridValueY());
  DaliExpects(
      residual < constants_.epsilon,
      "Site height is not integer multiple of grid value in Y"
  );
  tech_.row_height_set_ = true;
  tech_.row_height_ = row_height;
}

double Circuit::RowHeightMicronUnit() const {
  return tech_.row_height_;
}

int32_t Circuit::RowHeightGridUnit() const {
  DaliExpects(tech_.row_height_set_,
              "Row height not set, cannot retrieve its value: Circuit::RowHeightGridUnit()\n");
  return (int32_t) std::round(tech_.row_height_ / GridValueY());
}

std::vector<MetalLayer> &Circuit::Metals() {
  return tech_.metal_list_;
}

std::unordered_map<std::string, int32_t> &Circuit::MetalNameMap() {
  return tech_.metal_name_map_;
}

bool Circuit::IsMetalLayerExisting(std::string const &metal_name) {
  return tech_.metal_name_map_.find(metal_name)
      != tech_.metal_name_map_.end();
}

int32_t Circuit::GetMetalLayerId(std::string const &metal_name) {
  DaliExpects(IsMetalLayerExisting(metal_name),
              "MetalLayer does not exist, cannot find it: " + metal_name);
  return tech_.metal_name_map_.find(metal_name)->second;
}

MetalLayer *Circuit::GetMetalLayerPtr(std::string const &metal_name) {
  DaliExpects(IsMetalLayerExisting(metal_name),
              "MetalLayer does not exist, cannot find it: " + metal_name);
  return &tech_.metal_list_[GetMetalLayerId(metal_name)];
}

MetalLayer *Circuit::AddMetalLayer(
    std::string const &metal_name,
    double width,
    double spacing,
    double min_area,
    double pitch_x,
    double pitch_y,
    MetalDirection metal_direction
) {
  DaliExpects(!IsMetalLayerExisting(metal_name),
              "MetalLayer exist, cannot create this MetalLayer again: "
                  + metal_name);
  int32_t map_size = tech_.metal_name_map_.size();
  auto ret = tech_.metal_name_map_.insert(
      std::unordered_map<std::string, int32_t>::value_type(metal_name, map_size)
  );
  std::pair<const std::string, int32_t> *name_id_pair_ptr = &(*ret.first);
  tech_.metal_list_.emplace_back(width, spacing, name_id_pair_ptr);
  tech_.metal_list_.back().SetMinArea(min_area);
  tech_.metal_list_.back().SetPitch(pitch_x, pitch_y);
  tech_.metal_list_.back().SetDirection(metal_direction);
  return &(tech_.metal_list_.back());
}

// report metal layer information for debugging purposes
void Circuit::ReportMetalLayers() {
  BOOST_LOG_TRIVIAL(info)
    << "Total MetalLayer: " << tech_.metal_list_.size() << "\n";
  for (auto &metal_layer : tech_.metal_list_) {
    metal_layer.Report();
  }
  BOOST_LOG_TRIVIAL(info) << "\n";
}

std::unordered_map<std::string, BlockType *> &Circuit::BlockTypeMap() {
  return tech_.block_type_map_;
}

bool Circuit::IsBlockTypeExisting(std::string const &block_type_name) {
  return tech_.block_type_map_.find(block_type_name)
      != tech_.block_type_map_.end();
}

BlockType *Circuit::GetBlockTypePtr(std::string const &block_type_name) {
  auto res = tech_.block_type_map_.find(block_type_name);
  if (res == tech_.block_type_map_.end()) {
    DaliExpects(false, "Cannot find block_type_name: " + block_type_name);
  }
  return res->second;
}

BlockType *Circuit::AddBlockType(
    std::string const &block_type_name,
    double width,
    double height
) {
  int32_t gridded_width = 0;
  int32_t gridded_height = 0;
  BlockTypeSizeMicrometerToGridValue(
      block_type_name, width, height, gridded_width, gridded_height
  );
  return AddBlockTypeWithGridUnit(
      block_type_name, gridded_width, gridded_height
  );
}

BlockType *Circuit::AddWellTapBlockType(
    std::string const &block_type_name,
    double width,
    double height
) {
  double residual = AbsResidual(width, GridValueX());
  DaliExpects(residual < constants_.epsilon,
              "BlockType width is not integer multiple of grid value in X: "
                  + block_type_name);

  residual = AbsResidual(height, GridValueY());
  DaliExpects(residual < constants_.epsilon,
              "BlockType height is not integer multiple of grid value in Y: "
                  + block_type_name);

  int32_t gridded_width = (int32_t) std::round(width / GridValueX());
  int32_t gridded_height = (int32_t) std::round(height / GridValueY());
  return AddWellTapBlockTypeWithGridUnit(
      block_type_name, gridded_width, gridded_height
  );
}

Pin *Circuit::AddBlkTypePin(
    BlockType *blk_type_ptr,
    std::string const &pin_name,
    bool is_input
) {
  DaliExpects(blk_type_ptr != nullptr, "Add a pin to a nullptr?");
  return blk_type_ptr->AddPin(pin_name, is_input);
}

void Circuit::ReportBlockType() {
  BOOST_LOG_TRIVIAL(info)
    << "Total BlockType: "
    << tech_.block_type_map_.size() << std::endl;
  for (auto &pair : tech_.block_type_map_) {
    pair.second->Report();
  }
  BOOST_LOG_TRIVIAL(info) << "\n";
}

void Circuit::CopyBlockType(Circuit &circuit) {
  BlockType *blk_type = nullptr;
  BlockType *blk_type_new = nullptr;
  for (auto &item : circuit.tech_.block_type_map_) {
    blk_type = item.second;
    auto type_name = blk_type->Name();
    if (type_name == "PIN") continue;
    blk_type_new = AddBlockTypeWithGridUnit(
        type_name,
        blk_type->Width(),
        blk_type->Height()
    );
    for (auto &pin : blk_type->PinList()) {
      blk_type_new->AddPin(pin.Name(), pin.OffsetX(), pin.OffsetY());
    }
  }
}

void Circuit::SetUnitsDistanceMicrons(int32_t distance_microns) {
  DaliExpects(distance_microns > 0, "Negative distance micron?");
  design_.distance_microns_ = distance_microns;
}

int32_t Circuit::DistanceMicrons() const {
  return design_.distance_microns_;
}

void Circuit::SetDieArea(int32_t lower_x, int32_t lower_y, int32_t upper_x, int32_t upper_y) {
  DaliExpects(GridValueX() > 0 && GridValueY() > 0,
              "Need to set positive grid values before setting placement boundary");
  DaliExpects(design_.distance_microns_ > 0,
              "Need to set def_distance_microns before setting placement boundary using Circuit::SetDieArea()");
  int32_t factor_x = DistanceScaleFactorX();
  int32_t factor_y = DistanceScaleFactorY();

  // TODO, extract placement boundary from rows if they are any rows
  design_.die_area_offset_x_ = lower_x % factor_x;
  design_.die_area_offset_y_ = lower_y % factor_y;
  int32_t adjusted_lower_x = lower_x - design_.die_area_offset_x_;
  int32_t adjusted_lower_y = lower_y - design_.die_area_offset_y_;
  int32_t adjusted_upper_x = upper_x - design_.die_area_offset_x_;
  int32_t adjusted_upper_y = upper_y - design_.die_area_offset_y_;
  if (design_.die_area_offset_x_ != 0) {
    BOOST_LOG_TRIVIAL(info)
      << "left placement boundary is not on placement grid: \n"
      << "  shift left from " << lower_x << " to " << adjusted_lower_x << "\n"
      << "  shift right from " << upper_x << " to " << adjusted_upper_x << "\n";
  }
  if (design_.die_area_offset_y_ != 0) {
    BOOST_LOG_TRIVIAL(info)
      << "bottom placement boundary is not on placement grid: \n"
      << "  shift bottom from " << lower_y << " to " << adjusted_lower_y << "\n"
      << "  shift top from " << upper_y << " to " << adjusted_upper_y << "\n";
  }
  lower_x = adjusted_lower_x;
  lower_y = adjusted_lower_y;
  upper_x = adjusted_upper_x;
  upper_y = adjusted_upper_y;
  int32_t left = lower_x / factor_x;
  int32_t bottom = lower_y / factor_y;

  design_.die_area_offset_x_residual_ = upper_x % factor_x;
  design_.die_area_offset_y_residual_ = upper_y % factor_y;
  adjusted_upper_x = upper_x - design_.die_area_offset_x_residual_;
  adjusted_upper_y = upper_y - design_.die_area_offset_y_residual_;
  if (design_.die_area_offset_x_residual_ != 0) {
    BOOST_LOG_TRIVIAL(info)
      << "right placement boundary is not on placement grid: \n"
      << "  shrink right from " << upper_x << " to " << adjusted_upper_x
      << "\n";
  }
  if (design_.die_area_offset_y_residual_ != 0) {
    BOOST_LOG_TRIVIAL(info)
      << "top placement boundary is not on placement grid: \n"
      << "  shrink top from " << upper_y << " to " << adjusted_upper_y << "\n";
  }
  upper_x = adjusted_upper_x;
  upper_y = adjusted_upper_y;
  int32_t right = upper_x / factor_x;
  int32_t top = upper_y / factor_y;

  SetBoundary(left, bottom, right, top);
}

int32_t Circuit::RegionLLX() const {
  return design_.region_left_;
}

int32_t Circuit::RegionURX() const {
  return design_.region_right_;
}

int32_t Circuit::RegionLLY() const {
  return design_.region_bottom_;
}

int32_t Circuit::RegionURY() const {
  return design_.region_top_;
}

int32_t Circuit::RegionWidth() const {
  return design_.region_right_ - design_.region_left_;
}

int32_t Circuit::RegionHeight() const {
  return design_.region_top_ - design_.region_bottom_;
}

int32_t Circuit::DieAreaOffsetX() const {
  return design_.die_area_offset_x_;
}

int32_t Circuit::DieAreaOffsetY() const {
  return design_.die_area_offset_y_;
}

void Circuit::SetListCapacity(
    int32_t components_count,
    int32_t pins_count,
    int32_t nets_count
) {
  DaliExpects(components_count >= 0, "Negative number of components?");
  DaliExpects(pins_count >= 0, "Negative number of IOPINs?");
  DaliExpects(nets_count >= 0, "Negative number of NETs?");
  DaliExpects(components_count <= INT_MAX - pins_count,
              "Too many components and pins, total number larger than INT_MAX");
  design_.blocks_.reserve(components_count + pins_count);
  design_.iopins_.reserve(pins_count);
  design_.nets_.reserve(nets_count);
}

std::vector<Block> &Circuit::Blocks() {
  return design_.blocks_;
}

bool Circuit::IsBlockExisting(std::string const &block_name) {
  return !(design_.blk_name_id_map_.find(block_name)
      == design_.blk_name_id_map_.end());
}

int32_t Circuit::GetBlockId(std::string const &block_name) {
  auto ret = design_.blk_name_id_map_.find(block_name);
  if (ret == design_.blk_name_id_map_.end()) {
    DaliExpects(false, "Block name does not exist, cannot get index "
        + block_name);
  }
  return ret->second;
}

Block *Circuit::GetBlockPtr(std::string const &block_name) {
  return &design_.blocks_[GetBlockId(block_name)];
}

void Circuit::AddBlock(
    std::string const &block_name,
    std::string const &block_type_name,
    double llx,
    double lly,
    PlaceStatus place_status,
    BlockOrient orient,
    bool is_real_cel
) {
  BlockType *block_type = GetBlockTypePtr(block_type_name);
  AddBlock(
      block_name, block_type, llx, lly, place_status, orient, is_real_cel
  );
}

void Circuit::UpdateTotalBlkArea() {
  design_.tot_white_space_ =
      (unsigned long long) (design_.region_right_ - design_.region_left_) *
          (unsigned long long) (design_.region_top_ - design_.region_bottom_);
  RectI bin_rect(
      design_.region_left_, design_.region_bottom_,
      design_.region_right_, design_.region_top_
  );
  std::vector<Block> &blocks = Blocks();

  std::vector<RectI> rects;
  for (auto &blk : blocks) {
    if (blk.IsMovable()) continue;
    RectI fixed_blk_rect(
        static_cast<int32_t>(std::round(blk.LLX())),
        static_cast<int32_t>(std::round(blk.LLY())),
        static_cast<int32_t>(std::round(blk.URX())),
        static_cast<int32_t>(std::round(blk.URY()))
    );
    if (bin_rect.IsOverlap(fixed_blk_rect)) {
      rects.push_back(bin_rect.GetOverlapRect(fixed_blk_rect));
    }
  }

  design_.tot_fixed_blk_cover_area_ = GetCoverArea(rects);
  if (design_.tot_white_space_ < design_.tot_fixed_blk_cover_area_) {
    DaliExpects(false,
                "Fixed blocks takes more space than available space? "
                    + std::to_string(design_.tot_blk_area_) + " "
                    + std::to_string(design_.tot_fixed_blk_cover_area_)
    );
  }
  design_.tot_white_space_ -= design_.tot_fixed_blk_cover_area_;
  design_.tot_blk_area_ = design_.tot_fixed_blk_cover_area_
      + design_.tot_mov_blk_area_;
}

void Circuit::ReportBlockList() {
  BOOST_LOG_TRIVIAL(info) << "Total Block: " << design_.blocks_.size()
                          << "\n";
  for (auto &block : design_.blocks_) {
    block.Report();
  }
  BOOST_LOG_TRIVIAL(info) << "\n";
}

void Circuit::ReportBlockMap() {
  for (auto &it : design_.blk_name_id_map_) {
    BOOST_LOG_TRIVIAL(info) << it.first << " " << it.second << "\n";
  }
}

std::vector<IoPin> &Circuit::IoPins() {
  return design_.iopins_;
}

bool Circuit::IsIoPinExisting(std::string const &iopin_name) {
  return !(design_.iopin_name_id_map_.find(iopin_name)
      == design_.iopin_name_id_map_.end());
}

int32_t Circuit::GetIoPinId(std::string const &iopin_name) {
  auto ret = design_.iopin_name_id_map_.find(iopin_name);
  if (ret == design_.iopin_name_id_map_.end()) {
    DaliExpects(false, "IoPin name does not exist, cannot get index "
        + iopin_name);
  }
  return ret->second;
}

IoPin *Circuit::GetIoPinPtr(std::string const &iopin_name) {
  return &design_.iopins_[GetIoPinId(iopin_name)];
}

IoPin *Circuit::AddIoPin(
    std::string const &iopin_name,
    PlaceStatus place_status,
    SignalUse signal_use,
    SignalDirection signal_direction,
    double lx,
    double ly
) {
  IoPin *io_pin = nullptr;
  if (place_status == UNPLACED) {
    io_pin = AddUnplacedIoPin(iopin_name);
  } else {
    io_pin = AddPlacedIOPin(iopin_name, lx, ly);
    io_pin->SetInitPlaceStatus(place_status);
  }
  io_pin->SetSigUse(signal_use);
  io_pin->SetSigDirection(signal_direction);
  return io_pin;
}

void Circuit::AddIoPinFromPhyDB(phydb::IOPin &iopin) {
  std::string iopin_name(iopin.GetName());
  auto location = iopin.GetLocation();
  int32_t iopin_x = LocPhydb2DaliX(location.x);
  int32_t iopin_y = LocPhydb2DaliY(location.y);
  bool is_loc_set =
      (iopin.GetPlacementStatus() != phydb::PlaceStatus::UNPLACED);
  auto sig_use = SignalUse(iopin.GetUse());
  auto sig_dir = SignalDirection(iopin.GetDirection());

  if (is_loc_set) {
    IoPin *pin = AddIoPin(
        iopin_name, PLACED, sig_use, sig_dir, iopin_x, iopin_y
    );
    std::string layer_name = iopin.GetLayerName();
    MetalLayer *metal_layer = GetMetalLayerPtr(layer_name);
    pin->SetLayerPtr(metal_layer);
  } else {
    AddIoPin(iopin_name, UNPLACED, sig_use, sig_dir);
  }
}

void Circuit::ReportIOPin() {
  BOOST_LOG_TRIVIAL(info)
    << "Total IOPin: " << design_.iopins_.size() << "\n";
  for (auto &iopin : design_.iopins_) {
    iopin.Report();
  }
  BOOST_LOG_TRIVIAL(info) << "\n";
}

std::vector<Net> &Circuit::Nets() {
  return design_.nets_;
}

bool Circuit::IsNetExisting(std::string const &net_name) {
  return !(design_.net_name_id_map_.find(net_name)
      == design_.net_name_id_map_.end());
}

int32_t Circuit::GetNetId(std::string const &net_name) {
  auto ret = design_.net_name_id_map_.find(net_name);
  if (ret == design_.net_name_id_map_.end()) {
    DaliExpects(false, "Net name does not exist, cannot find index "
        + net_name);
  }
  return ret->second;
}

Net *Circuit::GetNetPtr(std::string const &net_name) {
  return &design_.nets_[GetNetId(net_name)];
}

/****
 * Returns a pointer to the newly created Net.
 * @param net_name: name of the net
 * @param capacity: maximum number of possible pins in this net
 * @param weight:   weight of this net, if less than 0, then default net weight will be used
 * ****/
Net *Circuit::AddNet(std::string const &net_name, int32_t capacity, double weight) {
  DaliExpects(!IsNetExisting(net_name),
              "Net exists, cannot create this net again: " + net_name);
  int32_t map_size = (int32_t) design_.net_name_id_map_.size();
  design_.net_name_id_map_.insert(
      std::unordered_map<std::string, int32_t>::value_type(net_name, map_size)
  );
  std::pair<const std::string, int32_t> *name_id_pair_ptr =
      &(*design_.net_name_id_map_.find(net_name));
  if (weight < 0) {
    weight = constants_.normal_net_weight;
  }
  design_.nets_.emplace_back(name_id_pair_ptr, capacity, weight);
  return &design_.nets_.back();
}

void Circuit::AddIoPinToNet(
    std::string const &iopin_name,
    std::string const &net_name
) {
  IoPin *iopin = GetIoPinPtr(iopin_name);
  Net *io_net = GetNetPtr(net_name);
  iopin->SetNetPtr(io_net);
  io_net->AddIoPin(iopin);
  if (iopin->IsPrePlaced()) {
    Block *blk_ptr = GetBlockPtr(iopin_name);
    Pin *pin = &(blk_ptr->TypePtr()->PinList()[0]);
    io_net->AddBlkPinPair(blk_ptr, pin);
  }
}

void Circuit::AddBlkPinToNet(
    std::string const &blk_name,
    std::string const &pin_name,
    std::string const &net_name
) {
  Block *blk_ptr = GetBlockPtr(blk_name);
  Pin *pin = blk_ptr->TypePtr()->GetPinPtr(pin_name);
  Net *net = GetNetPtr(net_name);
  net->AddBlkPinPair(blk_ptr, pin);
}

void Circuit::ReportNetList() {
  BOOST_LOG_TRIVIAL(info)
    << "Total Net: " << design_.nets_.size() << "\n";
  for (auto &net : design_.nets_) {
    BOOST_LOG_TRIVIAL(info)
      << "  " << net.Name() << "  " << net.Weight() << "\n";
    for (auto &block_pin : net.BlockPins()) {
      BOOST_LOG_TRIVIAL(info)
        << "\t" << " ("
        << block_pin.BlockName() << " "
        << block_pin.PinName() << ") "
        << "\n";
    }
  }
  BOOST_LOG_TRIVIAL(info) << "\n";
}

void Circuit::ReportNetMap() {
  for (auto &it : design_.net_name_id_map_) {
    BOOST_LOG_TRIVIAL(info) << it.first << " " << it.second << "\n";
  }
}

void Circuit::InitNetFanoutHistogram(std::vector<size_t> *histo_x) {
  design_.InitNetFanOutHistogram(histo_x);
  design_.net_histogram_.hpwl_unit = GridValueX();
}

void Circuit::UpdateNetHPWLHistogram() {
  size_t sz = design_.net_histogram_.buckets.size();
  design_.net_histogram_.sum_hpwls.assign(sz, 0);
  design_.net_histogram_.ave_hpwls.assign(sz, 0);
  design_.net_histogram_.min_hpwls.assign(sz, DBL_MAX);
  design_.net_histogram_.max_hpwls.assign(sz, -DBL_MAX);

  double grid_x_y_ratio = GridValueY() / GridValueX();
  for (auto &net : design_.nets_) {
    size_t net_size = net.PinCnt();
    double hpwl_x = net.WeightedHPWLX();
    double hpwl_y = net.WeightedHPWLY() * grid_x_y_ratio;
    design_.UpdateNetHPWLHistogram(net_size, hpwl_x + hpwl_y);
  }

  design_.net_histogram_.tot_hpwl = 0;
  for (size_t i = 0; i < sz; ++i) {
    design_.net_histogram_.tot_hpwl += design_.net_histogram_.sum_hpwls[i];
  }
}

void Circuit::ReportNetFanoutHistogram() {
  UpdateNetHPWLHistogram();
  design_.ReportNetFanOutHistogram();
}

void Circuit::ReportBriefSummary() {
  PrintHorizontalLine();
  BOOST_LOG_TRIVIAL(info)
    << "Circuit brief summary:\n";
  BOOST_LOG_TRIVIAL(info)
    << "  movable blocks: " << TotMovBlkCnt() << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "  fixed blocks:   " << design_.tot_fixed_blk_num_ << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "  blocks:         " << TotBlkCnt() << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "  iopins:         " << design_.iopins_.size() << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "  nets:           " << design_.nets_.size() << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "  grid size x/y:  " << GridValueX() << "/" << GridValueY() << "um\n";
  BOOST_LOG_TRIVIAL(info)
    << "  total movable blk area: " << design_.tot_mov_blk_area_ << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "  total white space     : " << design_.tot_white_space_ << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "  total block area      : " << design_.tot_blk_area_ << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "  total space: "
    << (long long) RegionWidth() * (long long) RegionHeight() << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "    left:   " << RegionLLX() << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "    right:  " << RegionURX() << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "    bottom: " << RegionLLY() << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "    top:    " << RegionURY() << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "  average movable width/height: "
    << AveMovBlkWidth() << "/" << AveMovBlkHeight() << "um\n";
  BOOST_LOG_TRIVIAL(info)
    << "  white space utility: " << WhiteSpaceUsage() << "\n";
  ReportHPWL();
}

void Circuit::SetNwellParams(
    double width,
    double spacing,
    double op_spacing,
    double max_plug_dist,
    double overhang
) {
  tech_.nwell_layer_.SetParams(
      width, spacing, op_spacing, max_plug_dist, overhang
  );
  tech_.n_set_ = true;
}

void Circuit::SetPwellParams(
    double width,
    double spacing,
    double op_spacing,
    double max_plug_dist,
    double overhang
) {
  tech_.pwell_layer_.SetParams(
      width, spacing, op_spacing, max_plug_dist, overhang
  );
  tech_.p_set_ = true;
}

// TODO: discuss with Rajit about the necessity of the parameter ANY_SPACING in CELL file.
void Circuit::SetLegalizerSpacing(double same_spacing, double any_spacing) {
  tech_.same_diff_spacing_ = same_spacing;
  tech_.any_diff_spacing_ = any_spacing;
}

BlockTypeWell *Circuit::AddBlockTypeWell(std::string const &blk_type_name) {
  BlockType *blk_type_ptr = GetBlockTypePtr(blk_type_name);
  return AddBlockTypeWell(blk_type_ptr);
}

// TODO: discuss with Rajit about the necessity of having N/P-wells not fully covering the prBoundary of a given cell.
void Circuit::SetWellRect(
    std::string const &blk_type_name,
    bool is_n,
    double lx,
    double ly,
    double ux,
    double uy
) {
  // check if well width is smaller than max_plug_distance
  double width = ux - lx;
  double max_plug_distance = 0;
  if (is_n) {
    DaliExpects(
        tech_.n_set_,
        "Nwell layer not found, cannot set well rect: " << blk_type_name
    );
    max_plug_distance = tech_.nwell_layer_.MaxPlugDist();
    DaliWarns(
        width > max_plug_distance,
        "BlockType has a Nwell wider than max_plug_distance, this may make well legalization fail: "
            << blk_type_name
    );
  } else {
    DaliExpects(
        tech_.p_set_,
        "Pwell layer not found, cannot set well rect: " << blk_type_name
    );
    max_plug_distance = tech_.pwell_layer_.MaxPlugDist();
    DaliWarns(
        width > max_plug_distance,
        "BlockType has a Pwell wider than max_plug_distance, this may make well legalization fail: "
            << blk_type_name
    );
  }

  // add well rect
  BlockType *blk_type_ptr = GetBlockTypePtr(blk_type_name);
  DaliExpects(
      blk_type_ptr != nullptr,
      "Cannot find BlockType with name: " << blk_type_name
  );
  double ly_residual = AbsResidual(ly, GridValueY());
  if (ly_residual > constants_.epsilon) {
    BOOST_LOG_TRIVIAL(debug)
      << "NOTE: ly of well rect for " << blk_type_name
      << " is not an integer multiple of grid value y\n"
      << "  ly: " << ly << ", grid value y: " << GridValueY() << "\n";
  }
  double uy_residual = AbsResidual(uy, GridValueY());
  if (uy_residual > constants_.epsilon) {
    BOOST_LOG_TRIVIAL(debug)
      << "NOTE: uy of well rect for " << blk_type_name
      << " is not an integer multiple of grid value y\n"
      << "  uy: " << uy << ", grid value y: " << GridValueY() << "\n";
  }
  int32_t lx_grid = int32_t(std::round(lx / GridValueX()));
  int32_t ly_grid = int32_t(std::round(ly / GridValueY()));
  int32_t ux_grid = int32_t(std::round(ux / GridValueX()));
  int32_t uy_grid = int32_t(std::round(uy / GridValueY()));

  BlockTypeWell *well = blk_type_ptr->WellPtr();
  DaliExpects(
      well != nullptr,
      "Well uninitialized for BlockType: " << blk_type_name
  );
  well->AddWellRect(is_n, lx_grid, ly_grid, ux_grid, uy_grid);
}

void Circuit::ReportWellShape() {
  for (auto &blk_type_well : tech_.multi_well_list_) {
    blk_type_well.Report();
  }
}

void Circuit::ReadMultiWellCell(std::string const &name_of_file) {
  std::ifstream ist(name_of_file.c_str());
  DaliExpects(ist.is_open(), "Cannot open input file " + name_of_file);

  std::string line;
  while (!ist.eof()) {
    getline(ist, line);
    if (line.empty()) continue;
    if (line.find("LAYER") != std::string::npos) {
      if (line.find("LEGALIZER") != std::string::npos) {
        std::vector<std::string> legalizer_fields;
        double same_diff_spacing = 0;
        double any_diff_spacing = 0;
        do {
          getline(ist, line);
          StrTokenize(line, legalizer_fields);
          if (legalizer_fields.size() != 2) {
            std::cout << "Expect: SPACING + Value, get: " + line
                      << std::endl;
            exit(1);
          }
          if (legalizer_fields[0] == "SAME_DIFF_SPACING") {
            try {
              same_diff_spacing = std::stod(legalizer_fields[1]);
            } catch (...) {
              std::cout << "Invalid stod conversion: " + line
                        << std::endl;
              exit(1);
            }
          } else if (legalizer_fields[0] == "ANY_DIFF_SPACING") {
            try {
              any_diff_spacing = std::stod(legalizer_fields[1]);
            } catch (...) {
              std::cout << "Invalid stod conversion: " + line
                        << std::endl;
              exit(1);
            }
          }
        } while (line.find("END LEGALIZER") == std::string::npos && !ist.eof());
        SetLegalizerSpacing(same_diff_spacing, any_diff_spacing);
      } else {
        std::vector<std::string> well_fields;
        StrTokenize(line, well_fields);
        bool is_n_well = (well_fields[1] == "nwell");
        if (!is_n_well) {
          if (well_fields[1] != "pwell") {
            std::cout << "Unknow N/P well type: " + well_fields[1] << std::endl;
            exit(1);
          }
        }
        std::string end_layer_flag = "END " + well_fields[1];
        double width = 0;
        double spacing = 0;
        double op_spacing = 0;
        double max_plug_dist = 0;
        double overhang = 0;
        do {
          if (line.find("MINWIDTH") != std::string::npos) {
            StrTokenize(line, well_fields);
            try {
              width = std::stod(well_fields[1]);
            } catch (...) {
              std::cout
                  << "Invalid stod conversion: " + well_fields[1]
                  << std::endl;
              exit(1);
            }
          } else if (line.find("OPPOSPACING") != std::string::npos) {
            StrTokenize(line, well_fields);
            try {
              op_spacing = std::stod(well_fields[1]);
            } catch (...) {
              std::cout
                  << "Invalid stod conversion: " + well_fields[1]
                  << std::endl;
              exit(1);
            }
          } else if (line.find("SPACING") != std::string::npos) {
            StrTokenize(line, well_fields);
            try {
              spacing = std::stod(well_fields[1]);
            } catch (...) {
              std::cout
                  << "Invalid stod conversion: " + well_fields[1]
                  << std::endl;
              exit(1);
            }
          } else if (line.find("MAXPLUGDIST") != std::string::npos) {
            StrTokenize(line, well_fields);
            try {
              max_plug_dist = std::stod(well_fields[1]);
            } catch (...) {
              std::cout
                  << "Invalid stod conversion: " + well_fields[1]
                  << std::endl;
              exit(1);
            }
          } else if (line.find("MAXPLUGDIST") != std::string::npos) {
            StrTokenize(line, well_fields);
            try {
              overhang = std::stod(well_fields[1]);
            } catch (...) {
              std::cout
                  << "Invalid stod conversion: " + well_fields[1]
                  << std::endl;
              exit(1);
            }
          } else {}
          getline(ist, line);
        } while (line.find(end_layer_flag) == std::string::npos
            && !ist.eof());
        if (is_n_well) {
          SetNwellParams(
              width, spacing, op_spacing,
              max_plug_dist, overhang
          );
        } else {
          SetPwellParams(
              width, spacing, op_spacing,
              max_plug_dist, overhang
          );
        }
      }
    }

    if (line.find("MACRO") != std::string::npos) {
      std::vector<std::string> macro_fields;
      StrTokenize(line, macro_fields);
      std::string end_macro_flag = "END " + macro_fields[1];
      BlockType *p_blk_type = GetBlockTypePtr(macro_fields[1]);
      tech_.multi_well_list_.emplace_back(p_blk_type);
      auto *well_ptr = &(tech_.multi_well_list_.back());
      do {
        getline(ist, line);
        if (line.find("REGION") != std::string::npos) {
          do {
            getline(ist, line);
            bool is_n = true;
            if (line.find("LAYER") != std::string::npos) {
              do {
                if (line.find("nwell") != std::string::npos) {
                  is_n = true;
                }
                if (line.find("pwell") != std::string::npos) {
                  is_n = false;
                }
                if (line.find("RECT") != std::string::npos) {
                  double lx = 0, ly = 0, ux = 0, uy = 0;
                  std::vector<std::string> shape_fields;
                  StrTokenize(line, shape_fields);
                  try {
                    lx = std::stod(shape_fields[1]);
                    ly = std::stod(shape_fields[2]);
                    ux = std::stod(shape_fields[3]);
                    uy = std::stod(shape_fields[4]);
                    double ly_residual = AbsResidual(ly, GridValueY());
                    if (ly_residual > constants_.epsilon) {
                      BOOST_LOG_TRIVIAL(trace)
                        << "WARNING: ly of well rect for " << macro_fields[1]
                        << " is not an integer multiple of grid value y\n"
                        << "  ly: " << ly << ", grid value y: " << GridValueY()
                        << "\n";
                    }
                    double uy_residual = AbsResidual(uy, GridValueY());
                    if (uy_residual > constants_.epsilon) {
                      BOOST_LOG_TRIVIAL(trace)
                        << "WARNING: uy of well rect for " << macro_fields[1]
                        << " is not an integer multiple of grid value y\n"
                        << "  uy: " << uy << ", grid value y: " << GridValueY()
                        << "\n";
                    }
                  } catch (...) {
                    std::cout << "Invalid stod conversion: " + line
                              << std::endl;
                    exit(1);
                  }
                  int32_t lx_grid = int32_t(std::round(lx / GridValueX()));
                  int32_t ly_grid = int32_t(std::round(ly / GridValueY()));
                  int32_t ux_grid = int32_t(std::round(ux / GridValueX()));
                  int32_t uy_grid = int32_t(std::round(uy / GridValueY()));
                  well_ptr->AddWellRect(
                      is_n, lx_grid, ly_grid, ux_grid, uy_grid
                  );
                }
                getline(ist, line);
              } while (line.find("END REGION") == std::string::npos
                  && !ist.eof());
            }
          } while (line.find("END REGION") == std::string::npos && !ist.eof());
        }
      } while (line.find(end_macro_flag) == std::string::npos && !ist.eof());
      //well_ptr->Report();
      well_ptr->CheckLegality();
    }
  }
  //ReportWellShape();
}

int32_t Circuit::MinBlkWidth() const {
  return design_.blk_min_width_;
}

int32_t Circuit::MaxBlkWidth() const {
  return design_.blk_max_width_;
}

int32_t Circuit::MinBlkHeight() const {
  return design_.blk_min_height_;
}

int32_t Circuit::MaxBlkHeight() const {
  return design_.blk_max_height_;
}

unsigned long long Circuit::TotBlkArea() const {
  return design_.tot_blk_area_;
}

int32_t Circuit::TotBlkCnt() const {
  return design_.real_block_count_;
}

int32_t Circuit::TotMovBlkCnt() const {
  return design_.tot_mov_blk_num_;
}

int32_t Circuit::TotFixedBlkCnt() const {
  return (int32_t) design_.blocks_.size() - design_.tot_mov_blk_num_;
}

double Circuit::AveBlkWidth() const {
  return double(design_.tot_width_) / double(TotBlkCnt());
}

double Circuit::AveBlkHeight() const {
  return double(design_.tot_height_) / double(TotBlkCnt());
}

double Circuit::AveBlkArea() const {
  return double(design_.tot_blk_area_) / double(TotBlkCnt());
}

double Circuit::AveMovBlkWidth() const {
  return double(design_.tot_mov_width_) / TotMovBlkCnt();
}

double Circuit::AveMovBlkHeight() const {
  return double(design_.tot_mov_height_) / TotMovBlkCnt();
}

double Circuit::AveMovBlkArea() const {
  return double(design_.tot_mov_blk_area_) / TotMovBlkCnt();
}

double Circuit::WhiteSpaceUsage() const {
  return double(design_.tot_mov_blk_area_) / double(design_.tot_white_space_);
}

void Circuit::NetSortBlkPin() {
  for (auto &net : design_.nets_) {
    net.SortBlkPinList();
  }
}

double Circuit::WeightedHPWLX() {
  double hpwl_x = 0;
  for (auto &net : design_.nets_) {
    hpwl_x += net.WeightedHPWLX();
  }
  return hpwl_x * GridValueX();
}

double Circuit::WeightedHPWLY() {
  double hpwly = 0;
  for (auto &net : design_.nets_) {
    hpwly += net.WeightedHPWLY();
  }
  return hpwly * GridValueY();
}

double Circuit::WeightedHPWL() {
  double hpwl_x = 0;
  double hpwl_y = 0;
  for (auto &net : design_.nets_) {
    hpwl_x += net.WeightedHPWLX();
    hpwl_y += net.WeightedHPWLY();
  }
  return hpwl_x * GridValueX() + hpwl_y * GridValueY();
}

void Circuit::ReportHPWL() {
  BOOST_LOG_TRIVIAL(info)
    << "  current weighted HPWL: " << WeightedHPWL() << "um\n";
}

double Circuit::WeightedBoundingBoxX() {
  double bbox_x = 0;
  for (auto &net : design_.nets_) {
    bbox_x += net.WeightedBboxX();
  }
  return bbox_x * GridValueX();
}

double Circuit::WeightedBoundingBoxY() {
  double bbox_y = 0;
  for (auto &net : design_.nets_) {
    bbox_y += net.WeightedBboxY();
  }
  return bbox_y * GridValueY();
}

double Circuit::WeightedBoundingBox() {
  return WeightedBoundingBoxX() + WeightedBoundingBoxY();
}

void Circuit::ReportBoundingBox() {
  BOOST_LOG_TRIVIAL(info)
    << "  current weighted bbox: " << WeightedBoundingBox() << " um\n";
}

void Circuit::ReportHPWLHistogramLinear(int32_t bin_num) {
  std::vector<double> hpwl_list;
  double min_hpwl = DBL_MAX;
  double max_hpwl = -DBL_MAX;
  hpwl_list.reserve(design_.nets_.size());
  double factor = GridValueY() / GridValueX();
  for (auto &net : design_.nets_) {
    double tmp_hpwl = net.WeightedHPWLX() + net.WeightedHPWLY() * factor;
    if (net.PinCnt() >= 1) {
      hpwl_list.push_back(tmp_hpwl);
      min_hpwl = std::min(min_hpwl, tmp_hpwl);
      max_hpwl = std::max(max_hpwl, tmp_hpwl);
    }
  }

  double step = (max_hpwl - min_hpwl) / bin_num;
  std::vector<int32_t> count(bin_num, 0);
  for (auto &hpwl : hpwl_list) {
    int32_t tmp_num = (int32_t) std::floor((hpwl - min_hpwl) / step);
    if (tmp_num == bin_num) {
      tmp_num = bin_num - 1;
    }
    ++count[tmp_num];
  }

  int32_t tot_count = design_.nets_.size();
  BOOST_LOG_TRIVIAL(info) << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "                  HPWL histogram (linear scale bins)\n";
  BOOST_LOG_TRIVIAL(info)
    << "===================================================================\n";
  BOOST_LOG_TRIVIAL(info) << "   HPWL interval         Count\n";
  for (int32_t i = 0; i < bin_num; ++i) {
    double lo = min_hpwl + step * i;
    double hi = lo + step;

    std::string buffer(1024, '\0');
    int32_t written_length =
        sprintf(&buffer[0], "  [%.1e, %.1e) %8d  ", lo, hi, count[i]);
    buffer.resize(written_length);

    int32_t percent = std::ceil(50 * count[i] / (double) tot_count);
    for (int32_t j = 0; j < percent; ++j) {
      buffer.push_back('*');
    }
    buffer.push_back('\n');
    BOOST_LOG_TRIVIAL(info) << buffer;
  }
  BOOST_LOG_TRIVIAL(info)
    << "===================================================================\n";
  BOOST_LOG_TRIVIAL(info) << " * HPWL unit, grid value in X: "
                          << GridValueX() << " um\n";
  BOOST_LOG_TRIVIAL(info) << "\n";
}

void Circuit::ReportHPWLHistogramLogarithm(int32_t bin_num) {
  std::vector<double> hpwl_list;
  double min_hpwl = DBL_MAX;
  double max_hpwl = -DBL_MAX;
  hpwl_list.reserve(design_.nets_.size());
  double factor = GridValueY() / GridValueX();
  for (auto &net : design_.nets_) {
    double tmp_hpwl = net.WeightedHPWLX() + net.WeightedHPWLY() * factor;
    if (tmp_hpwl > 0) {
      double log_hpwl = std::log10(tmp_hpwl);
      hpwl_list.push_back(log_hpwl);
      min_hpwl = std::min(min_hpwl, log_hpwl);
      max_hpwl = std::max(max_hpwl, log_hpwl);
    }
  }

  double step = (max_hpwl - min_hpwl) / bin_num;
  std::vector<int32_t> count(bin_num, 0);
  for (auto &hpwl : hpwl_list) {
    int32_t tmp_num = (int32_t) std::floor((hpwl - min_hpwl) / step);
    if (tmp_num == bin_num) {
      tmp_num = bin_num - 1;
    }
    ++count[tmp_num];
  }

  int32_t tot_count = design_.nets_.size();
  BOOST_LOG_TRIVIAL(info) << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "                  HPWL histogram (log scale bins)\n";
  BOOST_LOG_TRIVIAL(info)
    << "===================================================================\n";
  BOOST_LOG_TRIVIAL(info) << "   HPWL interval         Count\n";
  for (int32_t i = 0; i < bin_num; ++i) {
    double lo = std::pow(10, min_hpwl + step * i);
    double hi = std::pow(10, min_hpwl + step * (i + 1));

    std::string buffer(1024, '\0');
    int32_t written_length =
        sprintf(&buffer[0], "  [%.1e, %.1e) %8d  ", lo, hi, count[i]);
    buffer.resize(written_length);

    int32_t percent = std::ceil(50 * count[i] / (double) tot_count);
    for (int32_t j = 0; j < percent; ++j) {
      buffer.push_back('*');
    }
    buffer.push_back('\n');
    BOOST_LOG_TRIVIAL(info) << buffer;
  }
  BOOST_LOG_TRIVIAL(info)
    << "===================================================================\n";
  BOOST_LOG_TRIVIAL(info) << " * HPWL unit, grid value in X: "
                          << GridValueX() << " um\n";
  BOOST_LOG_TRIVIAL(info) << "\n";
}

double Circuit::HPWLCtoCX() {
  double hpwl_c2c_x = 0;
  for (auto &net : design_.nets_) {
    hpwl_c2c_x += net.HPWLCtoCX();
  }
  return hpwl_c2c_x * GridValueX();
}

double Circuit::HPWLCtoCY() {
  double hpwl_c2c_y = 0;
  for (auto &net : design_.nets_) {
    hpwl_c2c_y += net.HPWLCtoCY();
  }
  return hpwl_c2c_y * GridValueY();
}

double Circuit::HPWLCtoC() {
  return HPWLCtoCX() + HPWLCtoCY();
}

void Circuit::ReportHPWLCtoC() {
  BOOST_LOG_TRIVIAL(info) << "  Current HPWL: " << HPWLCtoC() << " um\n";
}

void Circuit::SaveOptimalRegionDistance(std::string const &file_name) {
  OptRegDist distance_calculator;
  distance_calculator.circuit_ = this;
  distance_calculator.SaveFile(file_name);
}

void Circuit::GenMATLABTable(
    std::string const &name_of_file,
    bool only_well_tap
) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

  // save place region
  SaveMatlabPatchRect(
      ost,
      RegionLLX(), RegionLLY(), RegionURX(), RegionURY(),
      true, 1, 1, 1
  );
  if (!only_well_tap) {
    // save ordinary cells
    for (auto &block : design_.blocks_) {
      SaveMatlabPatchRect(
          ost,
          block.LLX(), block.LLY(), block.URX(), block.URY(),
          true, 0, 1, 1
      );
    }
  }
  // save well-tap cells
  for (auto &block : design_.welltaps_) {
    SaveMatlabPatchRect(
        ost,
        block.LLX(), block.LLY(), block.URX(), block.URY(),
        true, 0, 1, 1
    );
  }
  ost.close();
}

void Circuit::GenMATLABWellTable(
    std::string const &name_of_file,
    bool only_well_tap
) {
  // generate MATLAB table for cell outlines
  std::string frame_file = name_of_file + "_outline.txt";
  GenMATLABTable(frame_file, only_well_tap);

  // generate MATLAB table for N/P-well shapes
  std::string unplug_file = name_of_file + "_unplug.txt";
  std::ofstream ost(unplug_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + unplug_file);
  if (!only_well_tap) {
    for (auto &block : design_.blocks_) {
      block.ExportWellToMatlabPatchRect(ost);
    }
  }
  for (auto &block : design_.welltaps_) {
    block.ExportWellToMatlabPatchRect(ost);
  }
  ost.close();
}

void Circuit::GenLongNetTable(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

  int32_t multi_factor = 5;
  double threshold = multi_factor * AveBlkHeight();
  int32_t count = 0;
  double ave_hpwl = 0;
  for (auto &net : design_.nets_) {
    double hpwl = net.WeightedHPWL();
    if (hpwl > threshold) {
      ave_hpwl += hpwl;
      ++count;
      for (auto &blk_pin : net.BlockPins()) {
        ost << blk_pin.AbsX() << "\t"
            << blk_pin.AbsY() << "\t";
      }
      ost << "\n";
    }
  }
  ave_hpwl /= count;
  BOOST_LOG_TRIVIAL(info)
    << "Long net report: \n"
    << "  threshold: " << multi_factor << " "
    << threshold << "\n"
    << "  count:     " << count << "\n"
    << "  ave_hpwl:  " << ave_hpwl << "\n";

  ost.close();
}

void Circuit::SaveCell(std::ofstream &ost, Block &blk) const {
  ost << "- "
      << blk.Name() << " "
      << blk.TypeName() << " + "
      << blk.StatusStr() << " "
      << "( "
      << LocDali2PhydbX(blk.LLX()) << " " << LocDali2PhydbY(blk.LLY())
      << " ) "
      << OrientStr(blk.Orient())
      << " ;\n";
}

void Circuit::SaveNormalCells(
    std::ofstream &ost,
    std::unordered_set<PlaceStatus> *filter_out
) {
  for (auto &blk : design_.blocks_) {
    if (blk.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
    if (filter_out != nullptr
        && filter_out->find(blk.Status()) != filter_out->end()) {
      continue;
    }
    SaveCell(ost, blk);
  }
}

void Circuit::SaveWellTapCells(std::ofstream &ost) {
  for (auto &blk : design_.welltaps_) {
    SaveCell(ost, blk);
  }
}

void Circuit::SaveCircuitWellCoverCell(
    std::ofstream &ost,
    std::string const &base_name
) const {
  ost << "- " << "npwells" << " "
      << base_name + "well" << " + "
      << "COVER "
      << "( "
      << LocDali2PhydbX(RegionLLX()) << " "
      << LocDali2PhydbY(RegionLLY())
      << " ) "
      << "N"
      << " ;\n";
}

void Circuit::SaveCircuitPpnpCoverCell(
    std::ofstream &ost,
    std::string const &base_name
) const {
  ost << "- " << "ppnps" << " "
      << base_name + "ppnp" << " + "
      << "COVER "
      << "( "
      << LocDali2PhydbX(RegionLLX()) << " "
      << LocDali2PhydbY(RegionLLY())
      << " ) "
      << "N"
      << " ;\n";
}

void Circuit::ExportNormalCells(std::ofstream &ost) {
  //count the number of normal cells
  size_t cell_count = 0;
  for (auto &block : design_.blocks_) { // skip dummy cells for I/O pins
    if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
    ++cell_count;
  }
  cell_count += design_.welltaps_.size();
  ost << "COMPONENTS " << cell_count << " ;\n";
  SaveNormalCells(ost);
  SaveWellTapCells(ost);
  ost << "END COMPONENTS\n\n";
}

void Circuit::ExportWellTapCells(std::ofstream &ost) {
  size_t cell_count = design_.welltaps_.size();
  ost << "COMPONENTS " << cell_count << " ;\n";
  SaveWellTapCells(ost);
  ost << "END COMPONENTS\n\n";
}

void Circuit::ExportNormalAndWellTapCells(
    std::ofstream &ost,
    std::string const &base_name
) {
  size_t cell_count = 0;
  for (auto &block : design_.blocks_) {
    if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
    ++cell_count;
  }
  cell_count += design_.welltaps_.size();
  cell_count += 1;
  ost << "COMPONENTS " << cell_count << " ;\n";
  SaveCircuitWellCoverCell(ost, base_name);
  SaveNormalCells(ost);
  SaveWellTapCells(ost);
  ost << "END COMPONENTS\n\n";
}

void Circuit::ExportNormalWellTapAndCoverCells(
    std::ofstream &ost,
    std::string const &base_name
) {
  size_t cell_count = 0;
  for (auto &block : design_.blocks_) {
    if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
    ++cell_count;
  }
  cell_count += design_.welltaps_.size();
  cell_count += 2;
  ost << "COMPONENTS " << cell_count << " ;\n";
  SaveCircuitWellCoverCell(ost, base_name);
  SaveCircuitPpnpCoverCell(ost, base_name);
  SaveNormalCells(ost);
  SaveWellTapCells(ost);
  ost << "END COMPONENTS\n\n";
}

void Circuit::ExportCellsExcept(
    std::ofstream &ost,
    std::unordered_set<PlaceStatus> *filter_out
) {
  size_t cell_count = 0;
  for (auto &block : design_.blocks_) {
    if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
    if (block.Status() == UNPLACED) continue;
    ++cell_count;
  }
  cell_count += design_.welltaps_.size();
  ost << "COMPONENTS " << cell_count << " ;\n";
  SaveNormalCells(ost, filter_out);
  SaveWellTapCells(ost);
  ost << "END COMPONENTS\n\n";
}

void Circuit::ExportCells(
    std::ofstream &ost,
    std::string const &base_name,
    int32_t mode
) {
  switch (mode) {
    case 0: { // no cells are saved
      ost << "COMPONENTS 0 ;\n";
      ost << "END COMPONENTS\n\n";
      break;
    }
    case 1: { // save all normal cells, regardless of the placement status
      ExportNormalCells(ost);
      break;
    }
    case 2: { // save only well tap cells
      ExportWellTapCells(ost);
      break;
    }
    case 3: { // save all normal cells + dummy cell for well filling
      ExportNormalAndWellTapCells(ost, base_name);
      break;
    }
    case 4: { // save all normal cells + dummy cell for well filling + dummy cell for n/p-plus filling
      ExportNormalWellTapAndCoverCells(ost, base_name);
      break;
    }
    case 5: { // save all placed and fixed cells
      std::unordered_set<PlaceStatus> filter_out;
      filter_out.insert(UNPLACED);
      ExportCellsExcept(ost, &filter_out);
      break;
    }
    default: {
      DaliExpects(false, "Unknown option, allowed value: 0-5");
    }
  }
}

void Circuit::SaveIoPin(
    std::ofstream &ost,
    IoPin &iopin,
    bool after_io_place
) const {
  ost << "- "
      << iopin.Name()
      << " + NET "
      << iopin.NetName()
      << " + DIRECTION "
      << iopin.SigDirectStr()
      << " + USE " << iopin.SigUseStr();
  if ((after_io_place && iopin.IsPlaced())
      || (!after_io_place && iopin.IsPrePlaced())) {
    std::string const &metal_name = iopin.LayerName();
    ost << "\n  + LAYER "
        << metal_name
        << " ( "
        << iopin.GetShape().LLX() * design_.distance_microns_ << " "
        << iopin.GetShape().LLY() * design_.distance_microns_ << " ) "
        << " ( "
        << iopin.GetShape().URX() * design_.distance_microns_ << " "
        << iopin.GetShape().URY() * design_.distance_microns_ << " ) ";
    ost << "\n  + PLACED ( "
        << LocDali2PhydbX(iopin.X())
        << " "
        << LocDali2PhydbY(iopin.Y())
        << " ) ";
    if (iopin.X() == design_.region_left_) {
      ost << "E";
    } else if (iopin.X() == design_.region_right_) {
      ost << "W";
    } else if (iopin.Y() == design_.region_bottom_) {
      ost << "N";
    } else {
      ost << "S";
    }
  }
  ost << " ;\n";
}

void Circuit::ExportIoPinsInfoAfterIoPlacement(std::ofstream &ost) {
  ost << "PINS " << design_.iopins_.size() << " ;\n";
  DaliExpects(!tech_.metal_list_.empty(),
              "Need metal layer info to generate PIN location\n");
  for (auto &iopin : design_.iopins_) {
    bool after_io_place = true;
    SaveIoPin(ost, iopin, after_io_place);
  }
  ost << "END PINS\n\n";
}

void Circuit::ExportIoPinsInfoBeforeIoPlacement(std::ofstream &ost) {
  ost << "PINS " << design_.iopins_.size() << " ;\n";
  DaliExpects(!tech_.metal_list_.empty(),
              "Need metal layer info to generate PIN location\n");
  for (auto &iopin : design_.iopins_) {
    bool after_io_place = false;
    SaveIoPin(ost, iopin, after_io_place);
  }
  ost << "END PINS\n\n";
}

void Circuit::ExportIoPins(std::ofstream &ost, int32_t mode) {
  switch (mode) {
    case 0: { // no IOPINs are saved
      ost << "PINS 0 ;\n";
      break;
    }
    case 1: { // save all IOPINs
      ExportIoPinsInfoAfterIoPlacement(ost);
      break;
    }
    case 2: { // save all IOPINs with status before IO placement
      ExportIoPinsInfoBeforeIoPlacement(ost);
      break;
    }
    default: {
      DaliExpects(false, "Unknown option, allowed value: 0-2\n");
    }
  }
}

void Circuit::ExportAllNets(std::ofstream &ost) {
  ost << "NETS " << design_.nets_.size() << " ;\n";
  for (auto &net : design_.nets_) {
    ost << "- " << net.Name() << "\n";
    ost << " ";
    for (auto &iopin : net.IoPinPtrs()) {
      ost << " ( PIN " << iopin->Name() << " ) ";
    }
    for (auto &pin_pair : net.BlockPins()) {
      if (pin_pair.BlkPtr()->TypePtr() == tech_.io_dummy_blk_type_ptr_) {
        continue;
      }
      ost << " ( " << pin_pair.BlockName() << " "
          << pin_pair.PinName() << " ) ";
    }
    ost << "\n" << " ;\n";
  }
  ost << "END NETS\n\n";
}

void Circuit::ExportPowerNetsForWellTapCells(std::ofstream &ost) {
  ost << "\nNETS 2 ;\n";
  // GND
  ost << "- ggnndd\n";
  ost << " ";
  for (auto &block : design_.welltaps_) {
    ost << " ( " << block.Name() << " g0 )";
  }
  ost << "\n" << " ;\n";
  //Vdd
  ost << "- vvdddd\n";
  ost << " ";
  for (auto &block : design_.welltaps_) {
    ost << " ( " << block.Name() << " v0 )";
  }
  ost << "\n" << " ;\n";
  ost << "END NETS\n\n";
}

void Circuit::ExportNets(std::ofstream &ost, int32_t mode) {
  switch (mode) {
    case 0: { // no nets are saved
      ost << "NETS 0 ;\n";
      ost << "END NETS\n\n";
      break;
    }
    case 1: { // save all nets
      ExportAllNets(ost);
      break;
    }
    case 2: { // save nets containing saved cells and IOPINs
      DaliExpects(false, "This part has not been implemented\n");
      break;
    }
    case 3: {// save power nets for well tap cell
      ExportPowerNetsForWellTapCells(ost);
      break;
    }
    default: {
      DaliExpects(false, "Unknown option, allowed value: 0-3\n");
    }
  }
}

// save def file for users.
/****
 * Universal function for saving DEF file
 * @param, output def file name, ".def" extension is added automatically
 * @param, input def file
 * @param, save_floorplan,
 *    case 0, floorplan not saved
 *    case 1, floorplan saved
 *    otherwise, report an error message
 * @param, save_cell,
 *    case 0, no cells are saved
 *    case 1, save all normal cells, regardless of the placement status
 *    case 2, save only well tap cells
 *    case 3, save all normal cells + dummy cell for well filling
 *    case 4, save all normal cells + dummy cell for well filling + dummy cell for n/p-plus filling
 *    case 5, save all placed cells
 *    otherwise, report an error message
 * @param, save_iopin
 *    case 0, no IOPINs are saved
 *    case 1, save all IOPINs
 *    case 2, save all IOPINs with status before IO placement
 *    otherwise, report an error message
 * @param, save_net
 *    case 0, no nets are saved
 *    case 1, save all nets
 *    case 2, save nets containing saved cells and IOPINs
 *    case 3, save power nets for well tap cell
 *    otherwise, report an error message
 * ****/
void Circuit::SaveDefFile(
    std::string const &base_name,
    std::string const &name_padding,
    std::string const &def_file_name,
    [[maybe_unused]]int32_t save_floorplan,
    int32_t save_cell,
    int32_t save_iopin,
    int32_t save_net
) {
  std::string file_name = base_name + name_padding + ".def";
  BOOST_LOG_TRIVIAL(info) << "Writing DEF file: " << file_name << "\n";
  std::ofstream ost(file_name.c_str());
  DaliExpects(ost.is_open(), "Cannot open file " + file_name);
  std::ifstream ist(def_file_name.c_str());
  DaliExpects(ist.is_open(), "Cannot open file " + def_file_name);

  // title of this DEF file
  using std::chrono::system_clock;
  system_clock::time_point today = system_clock::now();
  std::time_t tt = system_clock::to_time_t(today);
  ost << "##################################################\n";
  ost << "#  created by: Dali, build time: " << __DATE__ << " " << __TIME__
      << "\n";
  ost << "#  time: " << ctime(&tt);
  ost << "##################################################\n";

  std::string line;
  // 1. floor-plan
  while (true) {
    getline(ist, line);
    if (line.find("COMPONENTS") != std::string::npos || ist.eof()) {
      break;
    }
    ost << line << "\n";
  }
  // 2. COMPONENT
  ExportCells(ost, base_name, save_cell);
  // 3. PIN
  ExportIoPins(ost, save_iopin);
  // 4. NET
  ExportNets(ost, save_net);

  ost << "END DESIGN\n";
}

void Circuit::SaveDefFileComponent(
    std::string const &name_of_file,
    std::string const &def_file_name
) {
  std::string file_name = name_of_file;
  BOOST_LOG_TRIVIAL(info) << "Writing DEF file: " << file_name << "\n";
  std::ofstream ost(file_name.c_str());
  DaliExpects(ost.is_open(), "Cannot open file " + file_name);
  std::ifstream ist(def_file_name.c_str());
  DaliExpects(ist.is_open(), "Cannot open file " + def_file_name);

  // title of this DEF file
  using std::chrono::system_clock;
  system_clock::time_point today = system_clock::now();
  std::time_t tt = system_clock::to_time_t(today);
  ost << "##################################################\n";
  ost << "#  created by: Dali, build time: " << __DATE__ << " " << __TIME__
      << "\n";
  ost << "#  time: " << ctime(&tt);
  ost << "##################################################\n";

  std::string line;
  // copy everything before COMPONENTS
  while (true) {
    getline(ist, line);
    if (line.find("COMPONENTS") != std::string::npos || ist.eof()) {
      break;
    }
    ost << line << "\n";
  }
  // COMPONENT
  ExportCells(ost, "", 1);

  // copy everything after END COMPONENTS
  while (true) {
    getline(ist, line);
    if (line.find("END COMPONENTS") != std::string::npos || ist.eof()) {
      break;
    }
  }
  while (true) {
    getline(ist, line);
    if (ist.eof()) {
      break;
    }
    ost << line << "\n";
  }
}

void Circuit::SaveBookshelfNode(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);
  ost << "# this line is here just for ntuplace to recognize this file \n\n";
  ost << "NumNodes : \t\t" << design_.tot_mov_blk_num_ << "\n"
      << "NumTerminals : \t\t"
      << design_.blocks_.size() - design_.tot_mov_blk_num_ << "\n";
  for (auto &block : design_.blocks_) {
    ost << "\t" << block.Name()
        << "\t" << block.Width() * design_.distance_microns_ * GridValueX()
        << "\t" << block.Height() * design_.distance_microns_ * GridValueY()
        << "\n";
  }
}

void Circuit::SaveBookshelfNet(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);
  size_t num_pins = 0;
  for (auto &net : design_.nets_) {
    num_pins += net.BlockPins().size();
  }
  ost << "# this line is here just for ntuplace to recognize this file \n\n";
  ost << "NumNets : " << design_.nets_.size() << "\n"
      << "NumPins : " << num_pins << "\n\n";
  for (auto &net : design_.nets_) {
    ost << "NetDegree : " << net.BlockPins().size() << "   " << net.Name()
        << "\n";
    for (auto &pair : net.BlockPins()) {
      ost << "\t" << pair.BlockName() << "\t";
      if (pair.PinPtr()->IsInput()) {
        ost << "I : ";
      } else {
        ost << "O : ";
      }
      ost <<
          (pair.PinPtr()->OffsetX() - pair.BlkPtr()->TypePtr()->Width() / 2.0)
              * design_.distance_microns_ * GridValueX()
          << "\t"
          << (pair.PinPtr()->OffsetY()
              - pair.BlkPtr()->TypePtr()->Height() / 2.0)
              * design_.distance_microns_
              * GridValueY()
          << "\n";
    }
  }
}

void Circuit::SaveBookshelfPl(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);
  ost << "# this line is here just for ntuplace to recognize this file \n\n";
  for (auto &block : design_.blocks_) {
    ost << block.Name()
        << "\t" << int32_t(block.LLX() * design_.distance_microns_ * GridValueX())
        << "\t" << int32_t(block.LLY() * design_.distance_microns_ * GridValueY());
    if (block.IsMovable()) {
      ost << "\t:\tN\n";
    } else {
      ost << "\t:\tN\t/FIXED\n";
    }
  }
  ost.close();
}

void Circuit::SaveBookshelfScl(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);
  DaliExpects(false, "This part is to be implemented");
}

void Circuit::SaveBookshelfWts(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);
}

void Circuit::SaveBookshelfAux(std::string const &name_of_file) {
  std::string aux_name = name_of_file + ".aux";
  std::ofstream ost(aux_name.c_str());
  DaliExpects(ost.is_open(), "Cannot open file " + aux_name);
  ost << "RowBasedPlacement :  "
      << name_of_file << ".nodes  "
      << name_of_file << ".nets  "
      << name_of_file << ".wts  "
      << name_of_file << ".pl  "
      << name_of_file << ".scl";
}

void Circuit::LoadBookshelfPl(std::string const &name_of_file) {
  std::ifstream ist(name_of_file.c_str());
  DaliExpects(ist.is_open(), "Cannot open file " + name_of_file);

  std::string line;
  std::vector<std::string> res;
  double lx = 0;
  double ly = 0;

  while (!ist.eof()) {
    getline(ist, line);
    StrTokenize(line, res);
    if (res.size() >= 4) {
      if (IsBlockExisting(res[0])) {
        try {
          lx = std::stod(res[1]) / GridValueX() / design_.distance_microns_;
          ly = std::stod(res[2]) / GridValueY() / design_.distance_microns_;
          GetBlockPtr(res[0])->SetLoc(lx, ly);
        } catch (...) {
          DaliExpects(false, "Invalid stod conversion:\n\t" + line);
        }
      }
    }
  }
}

void Circuit::CreateFakeWellForStandardCell() {
  tech_.CreateFakeWellForStandardCell(phy_db_ptr_);
}

/****
 * Creates fake NP-well information for testing purposes
 * create fake N/P-well info for cells
 * ****/
void Circuit::LoadImaginaryCellFile() {
  // 1. create fake well tap cell
  std::string tap_cell_name("welltap_svt");
  BlockType *tmp_well_tap_cell = AddBlockTypeWithGridUnit(
      tap_cell_name,
      MinBlkWidth(),
      MinBlkHeight()
  );
  tech_.WellTapCellPtrs().push_back(tmp_well_tap_cell);

  // 2. create fake well parameters
  double fake_same_diff_spacing = 0;
  double fake_any_diff_spacing = 0;
  SetLegalizerSpacing(fake_same_diff_spacing, fake_any_diff_spacing);

  double width = MinBlkHeight() / 2.0 * GridValueY();
  double spacing = MinBlkWidth() * GridValueX();
  double op_spacing = MinBlkWidth() * GridValueX();
  double max_plug_dist = AveMovBlkWidth() * 10 * GridValueX();
  double overhang = 0;
  SetNwellParams(width, spacing, op_spacing, max_plug_dist, overhang);
  SetNwellParams(width, spacing, op_spacing, max_plug_dist, overhang);

  // 3. create fake NP-well geometries for each BlockType
  for (auto &pair : tech_.block_type_map_) {
    auto *blk_type = pair.second;
    BlockTypeWell *well = AddBlockTypeWell(blk_type);
    int32_t np_edge = blk_type->Height() / 2;
    well->AddPwellRect(0, 0, blk_type->Width(), np_edge);
    well->AddNwellRect(0, 0, blk_type->Width(), blk_type->Height());
  }
}

BlockType *Circuit::AddBlockTypeWithGridUnit(
    std::string const &block_type_name,
    int32_t width,
    int32_t height
) {
  DaliExpects(!IsBlockTypeExisting(block_type_name),
              "BlockType exist, cannot create this block type again: "
                  + block_type_name);
  auto ret = tech_.block_type_map_.insert(
      std::unordered_map<std::string, BlockType *>::value_type(
          block_type_name, nullptr
      )
  );
  auto tmp_ptr = new BlockType(&(ret.first->first), width, height);
  ret.first->second = tmp_ptr;
  if (tmp_ptr->Area() > INT_MAX) tmp_ptr->Report();
  return tmp_ptr;
}

BlockType *Circuit::AddWellTapBlockTypeWithGridUnit(
    std::string const &block_type_name,
    int32_t width,
    int32_t height
) {
  BlockType *well_tap_ptr =
      AddBlockTypeWithGridUnit(block_type_name, width, height);
  tech_.well_tap_cell_ptrs_.push_back(well_tap_ptr);
  return well_tap_ptr;
}

void Circuit::SetBoundary(int32_t left, int32_t bottom, int32_t right, int32_t top) {
  DaliExpects(right > left,
              "Right boundary is not larger than Left boundary?");
  DaliExpects(top > bottom,
              "Top boundary is not larger than Bottom boundary?");
  design_.region_left_ = left;
  design_.region_right_ = right;
  design_.region_bottom_ = bottom;
  design_.region_top_ = top;
  design_.die_area_set_ = true;
}

void Circuit::BlockTypeSizeMicrometerToGridValue(
    std::string const &block_type_name,
    double width,
    double height,
    int32_t &gridded_width,
    int32_t &gridded_height
) {
  double residual_x = AbsResidual(width, GridValueX());
  gridded_width = -1;
  if (residual_x < constants_.epsilon) {
    gridded_width = (int32_t) std::round(width / GridValueX());
  } else {
    gridded_width = (int32_t) std::ceil(width / GridValueX());
    BOOST_LOG_TRIVIAL(warning)
      << "BlockType width is not integer multiple of the grid value along X: "
      << block_type_name << "\n"
      << "    width: " << width << " um\n"
      << "    grid value x: " << GridValueX() << " um\n"
      << "    residual: " << residual_x << "\n"
      << "    adjusted up to: " << gridded_width << " * "
      << GridValueX() << " um\n";
  }

  double residual_y = AbsResidual(height, GridValueY());
  gridded_height = -1;
  if (residual_y < constants_.epsilon) {
    gridded_height = (int32_t) std::round(height / GridValueY());
  } else {
    gridded_height = (int32_t) std::ceil(height / GridValueY());
    BOOST_LOG_TRIVIAL(warning)
      << "BlockType height is not integer multiple of the grid value along Y: "
      << block_type_name << "\n"
      << "    height: " << height << " um\n"
      << "    grid value y: " << GridValueY() << " um\n"
      << "    residual: " << residual_y << "\n"
      << "    adjusted up to: " << gridded_height << " * "
      << GridValueY() << " um\n";
  }
}

void Circuit::AddBlock(
    std::string const &block_name,
    BlockType *block_type_ptr,
    double llx,
    double lly,
    PlaceStatus place_status,
    BlockOrient orient,
    bool is_real_cel
) {
  DaliExpects(design_.nets_.empty(),
              "Cannot add new Block, because net_list now is not empty");
  DaliExpects(design_.blocks_.size() < design_.blocks_.capacity(),
              "Cannot add new Block, because block list is full");
  DaliExpects(!IsBlockExisting(block_name),
              "Block exists, cannot create this block again: " + block_name);
  int32_t id = static_cast<int32_t>(design_.blk_name_id_map_.size());
  if (id < 0 || id > INT_MAX) {
    DaliExpects(false, "Cannot add more blocks, the limit is INT_MAX");
  }
  auto ret = design_.blk_name_id_map_.insert(
      std::unordered_map<std::string, int32_t>::value_type(block_name, id)
  );
  std::pair<const std::string, int32_t> *name_id_pair_ptr = &(*ret.first);
  design_.blocks_.emplace_back(
      block_type_ptr, name_id_pair_ptr, llx, lly, place_status, orient
  );

  if (!is_real_cel) return;
  // update statistics of blocks
  ++design_.real_block_count_;
  design_.tot_width_ += design_.blocks_.back().Width();
  design_.tot_height_ += design_.blocks_.back().Height();
  if (design_.blocks_.back().IsMovable()) {
    ++design_.tot_mov_blk_num_;
    auto old_tot_mov_area = design_.tot_mov_blk_area_;
    design_.tot_mov_blk_area_ += design_.blocks_.back().Area();
    DaliExpects(old_tot_mov_area <= design_.tot_mov_blk_area_,
                "Total Movable Block Area Overflow, choose a different MANUFACTURINGGRID/unit");
    design_.tot_mov_width_ += design_.blocks_.back().Width();
    design_.tot_mov_height_ += design_.blocks_.back().Height();
  } else {
    ++design_.tot_fixed_blk_num_;
  }
  if (design_.blocks_.back().Height() < design_.blk_min_height_) {
    design_.blk_min_height_ = design_.blocks_.back().Height();
  }
  if (design_.blocks_.back().Height() > design_.blk_max_height_) {
    design_.blk_max_height_ = design_.blocks_.back().Height();
  }
  if (design_.blocks_.back().Width() < design_.blk_min_width_) {
    design_.blk_min_width_ = design_.blocks_.back().Width();
  }
  if (design_.blocks_.back().Width() > design_.blk_min_width_) {
    design_.blk_max_width_ = design_.blocks_.back().Width();
  }
}

/****
 * This member function adds a dummy BlockType for IOPINs.
 * The name of this dummy BlockType is "PIN", and it contains one cell pin with name "pin".
 * The size of "PIN" BlockType is 0 (width) and 0(height).
 * The relative location of the only cell pin "pin" is (0,0) with size 0.
 * ****/
void Circuit::AddDummyIOPinBlockType() {
  std::string iopin_type_name("__PIN__");
  auto io_pin_type = AddBlockTypeWithGridUnit(iopin_type_name, 0, 0);
  std::string tmp_pin_name("pin");
  // TO-DO, the value of @param is_input may not be true
  Pin *pin = io_pin_type->AddPin(tmp_pin_name, true);
  pin->SetOffset(0, 0);
  tech_.io_dummy_blk_type_ptr_ = io_pin_type;
}

IoPin *Circuit::AddUnplacedIoPin(std::string const &iopin_name) {
  DaliExpects(design_.nets_.empty(),
              "Cannot add new IOPIN, because net_list now is not empty");
  DaliExpects(!IsIoPinExisting(iopin_name),
              "IOPin exists, cannot create this IOPin again: " + iopin_name);
  int32_t map_size = design_.iopin_name_id_map_.size();
  auto ret = design_.iopin_name_id_map_.insert(
      std::unordered_map<std::string, int32_t>::value_type(iopin_name, map_size)
  );
  std::pair<const std::string, int32_t> *name_id_pair_ptr = &(*ret.first);
  design_.iopins_.emplace_back(name_id_pair_ptr);
  return &(design_.iopins_.back());
}

/****
 * @brief PLACED I/O pins are treated as fixed blocks with no area
 * @param iopin_name: name of this I/O pin
 * @param lx: x location of this pin
 * @param ly: y location of this pin
 * @return a pointer to the newly added I/O pin
 */
IoPin *Circuit::AddPlacedIOPin(
    std::string const &iopin_name,
    double lx,
    double ly
) {
  DaliExpects(design_.nets_.empty(),
              "Cannot add new IOPIN, because net_list now is not empty");
  DaliExpects(!IsIoPinExisting(iopin_name),
              "IOPin exists, cannot create this IOPin again: " + iopin_name);
  int32_t map_size = (int32_t) design_.iopin_name_id_map_.size();
  auto ret = design_.iopin_name_id_map_.insert(
      std::unordered_map<std::string, int32_t>::value_type(iopin_name, map_size)
  );
  std::pair<const std::string, int32_t> *name_id_pair_ptr = &(*ret.first);
  design_.iopins_.emplace_back(name_id_pair_ptr, lx, ly);
  design_.pre_placed_io_count_ += 1;

  // add a dummy cell corresponding to this IOPIN to block_list.
  AddBlock(
      iopin_name, tech_.io_dummy_blk_type_ptr_, lx, ly, FIXED, N, false
  );

  return &(design_.iopins_.back());
}

BlockTypeWell *Circuit::AddBlockTypeWell(BlockType *blk_type) {
  tech_.multi_well_list_.emplace_back(blk_type);
  blk_type->SetWell(&(tech_.multi_well_list_.back()));
  return blk_type->WellPtr();
}

void Circuit::LoadTech(phydb::PhyDB *phy_db_ptr) {
  auto &phy_db_tech = *(phy_db_ptr->GetTechPtr());

  // 1. lef database microns and manufacturing grid
  DaliExpects(phy_db_tech.GetDatabaseMicron() > 0,
              "Bad DATABASE MICRONS from PhyDB");
  SetDatabaseMicrons(phy_db_tech.GetDatabaseMicron());
  BOOST_LOG_TRIVIAL(trace) << "  DATABASE MICRONS " << tech_.database_microns_
                           << "\n";
  if (phy_db_tech.GetManufacturingGrid() > constants_.epsilon) {
    SetManufacturingGrid(phy_db_tech.GetManufacturingGrid());
  } else {
    SetManufacturingGrid(1.0 / tech_.database_microns_);
  }
  BOOST_LOG_TRIVIAL(trace) << "  MANUFACTURINGGRID "
                           << tech_.manufacturing_grid_ << "\n";

  // 2. placement grid and metal layers
  double grid_value_x = 0;
  double grid_value_y = 0;
  bool is_placement_grid_set = phy_db_tech.GetPlacementGrids(
      grid_value_x,
      grid_value_y
  );
  if (is_placement_grid_set) {
    SetGridValue(grid_value_x, grid_value_y);
  } else {
    BOOST_LOG_TRIVIAL(info) << "  placement grid not set in PhyDB\n";
    BOOST_LOG_TRIVIAL(info) << "  checking sites\n";
    auto &sites = phy_db_tech.GetSitesRef();
    if (!sites.empty()) {
      grid_value_x = sites[0].GetWidth();
      grid_value_y = sites[0].GetHeight();
      BOOST_LOG_TRIVIAL(info) << "    width : " << grid_value_x << "um\n";
      BOOST_LOG_TRIVIAL(info) << "    height: " << grid_value_y << "um\n";
      SetGridValue(grid_value_x, grid_value_y);
      SetRowHeight(grid_value_y);
    } else {
      BOOST_LOG_TRIVIAL(info) << "  no sites found\n";
    }
  }

  for (auto &layer : phy_db_tech.GetLayersRef()) {
    if (layer.GetType() == phydb::LayerType::ROUTING) {
      std::string layer_name = layer.GetName();

      double pitch_x = -1, pitch_y = -1;
      layer.GetPitch(pitch_x, pitch_y);

      double min_width = layer.GetMinWidth();
      if (min_width <= 0) {
        min_width = layer.GetWidth();
      }
      DaliExpects(
          min_width > 0,
          "MinWidth and Width not found in PhyDB for layer: " + layer_name
      );

      double min_spacing = 0;
      auto &spacing_table = *(layer.GetSpacingTable());
      if (spacing_table.GetNRow() >= 1 && spacing_table.GetNCol() >= 1) {
        min_spacing = spacing_table.GetSpacingAt(0, 0);
      } else {
        min_spacing = layer.GetSpacing();
      }
      if (min_spacing <= 0) {
        BOOST_LOG_TRIVIAL(warning)
          << "A valid min spacing is not found for layer: " + layer_name
          << ", use its min width instead\n";
        min_spacing = min_width;
      }

      double min_area = layer.GetArea();
      DaliExpects(
          min_area >= 0,
          "Dali expects a non-negative MinArea for layer: " + layer_name
      );

      auto direction = MetalDirection(layer.GetDirection());
      AddMetalLayer(
          layer_name, min_width, min_spacing, min_area,
          pitch_x, pitch_y, direction
      );
    }
  }
  if (!tech_.is_grid_set_) {
    SetGridFromMetalPitch();
  }

  // 3. load all macros
  for (auto &macro : phy_db_tech.GetMacrosRef()) {
    std::string macro_name(macro.GetName());
    double width = macro.GetWidth();
    double height = macro.GetHeight();
    BlockType *blk_type = nullptr;
    if (macro.GetClass() == phydb::MacroClass::CORE_WELLTAP) {
      blk_type = AddWellTapBlockType(macro_name, width, height);
    } else {
      blk_type = AddBlockType(macro_name, width, height);
    }
    auto &macro_pins = macro.GetPinsRef();
    for (auto &pin : macro_pins) {
      std::string pin_name(pin.GetName());
      //if (pin_name == "Vdd" || pin_name == "GND") continue;

      bool is_input = true;
      auto pin_direction = pin.GetDirection();
      is_input = (pin_direction == phydb::SignalDirection::INPUT);
      Pin *new_pin = blk_type->AddPin(pin_name, is_input);

      auto &layer_rects = pin.GetLayerRectRef();
      DaliExpects(
          !layer_rects.empty(),
          "No physical pins, Macro: "
              << blk_type->Name() << ", pin: " << pin_name
      );

      auto bbox = pin.GetBoundingBox();
      double llx = LengthPhydb2DaliX(bbox.LLX());
      double urx = LengthPhydb2DaliX(bbox.URX());
      double lly = LengthPhydb2DaliY(bbox.LLY());
      double ury = LengthPhydb2DaliY(bbox.URY());
      new_pin->SetOffset(
          (llx + urx) / 2.0,
          (lly + ury) / 2.0
      );
      double bbox_width = LengthPhydb2DaliX(bbox.GetWidth());
      double bbox_height = LengthPhydb2DaliY(bbox.GetHeight());
      new_pin->SetBoundingBoxSize(bbox_width, bbox_height);
    }
  }
}

void Circuit::LoadDesign(phydb::PhyDB *phy_db_ptr) {
  auto &phy_db_design = *(phy_db_ptr->GetDesignPtr());

  // 1. reserve space for COMPONENTS, IOPINS, and NETS
  auto &components = phy_db_design.GetComponentsRef();
  int32_t components_count = (int32_t) components.size();

  auto &iopins = phy_db_design.GetIoPinsRef();
  int32_t pins_count = (int32_t) iopins.size();

  auto &nets = phy_db_design.GetNetsRef();
  int32_t nets_count = (int32_t) nets.size();
  SetListCapacity(components_count, pins_count, nets_count);

  // 2. load UNITS DISTANCE MICRONS and DIEAREA
  SetUnitsDistanceMicrons(phy_db_design.GetUnitsDistanceMicrons());
  auto die_area = phy_db_design.GetDieArea();
  SetDieArea(die_area.LLX(), die_area.LLY(), die_area.URX(), die_area.URY());

  // 3. load all components
  for (auto &comp : components) {
    std::string blk_name(comp.GetName());
    std::string blk_type_name(comp.GetMacro()->GetName());
    auto location = comp.GetLocation();
    int32_t llx = location.x;
    int32_t lly = location.y;
    double lx = std::round(LocPhydb2DaliX(llx));
    double ly = std::round(LocPhydb2DaliY(lly));
    auto place_status = PlaceStatus(comp.GetPlacementStatus());
    auto orient = BlockOrient(comp.GetOrientation());
    AddBlock(blk_name, blk_type_name, lx, ly, place_status, orient);
  }

  // 4. load all IOPINs
  for (auto &iopin : iopins) {
    AddIoPinFromPhyDB(iopin);
  }

  // 5. load all NETs
  for (auto &net : nets) {
    std::string net_name(net.GetName());
    auto &net_pins = net.GetPinsRef();
    std::vector<int32_t> &iopin_ids = net.GetIoPinIdsRef();
    int32_t net_capacity = int32_t(net_pins.size() + iopin_ids.size());
    AddNet(net_name, net_capacity, design_.normal_signal_weight_);

    for (int32_t &id : iopin_ids) {
      AddIoPinToNet(iopins[id].GetName(), net_name);
    }
    int32_t sz = (int32_t) net_pins.size();
    for (int32_t i = 0; i < sz; ++i) {
      int32_t comp_id = net_pins[i].InstanceId();
      std::string comp_name =
          phy_db_ptr_->GetDesignPtr()->GetComponentsRef()[comp_id].GetName();
      int32_t pin_id = net_pins[i].PinId();
      std::string pin_name =
          phy_db_ptr_->GetDesignPtr()->GetComponentsRef()[comp_id].GetMacro()->GetPinsRef()[pin_id].GetName();
      AddBlkPinToNet(comp_name, pin_name, net_name);
    }
  }
}

void Circuit::LoadCell(phydb::PhyDB *phy_db_ptr) {
  auto &phy_db_tech = *(phy_db_ptr->GetTechPtr());
  if (!phy_db_tech.IsWellInfoSet()) {
    BOOST_LOG_TRIVIAL(info) << "N/P-Well layer info not found in PhyDB\n";
    return;
  }

  double same_diff_spacing = 0, any_diff_spacing = 0;
  phy_db_tech.GetDiffWellSpacing(same_diff_spacing, any_diff_spacing);
  SetLegalizerSpacing(same_diff_spacing, any_diff_spacing);

  auto *n_well_layer = phy_db_tech.GetNwellLayerPtr();
  if (n_well_layer != nullptr) {
    double width = n_well_layer->GetWidth();
    double spacing = n_well_layer->GetSpacing();
    double op_spacing = n_well_layer->GetOpSpacing();
    double max_plug_dist = n_well_layer->GetMaxPlugDist();
    double overhang = n_well_layer->GetOverhang();
    SetNwellParams(width, spacing, op_spacing, max_plug_dist, overhang);
  }

  auto *p_well_layer = phy_db_tech.GetPwellLayerPtr();
  if (p_well_layer != nullptr) {
    double width = p_well_layer->GetWidth();
    double spacing = p_well_layer->GetSpacing();
    double op_spacing = p_well_layer->GetOpSpacing();
    double max_plug_dist = p_well_layer->GetMaxPlugDist();
    double overhang = p_well_layer->GetOverhang();
    SetPwellParams(width, spacing, op_spacing, max_plug_dist, overhang);
  }

  for (auto &macro : phy_db_tech.GetMacrosRef()) {
    std::string macro_name(macro.GetName());
    phydb::MacroWell *macro_well = macro.GetWellPtr();
    DaliExpects(macro_well != nullptr,
                "No well info provided for MACRO: " + macro_name);
    AddBlockTypeWell(macro_name);

    auto *n_rect = macro_well->GetNwellRectPtr();
    auto *p_rect = macro_well->GetPwellRectPtr();
    if (n_rect == nullptr && p_rect == nullptr) {
      DaliExpects(false, "N/P-well geometries not provided for MACRO: "
          + macro_name);
    }
    if (n_rect != nullptr) {
      SetWellRect(
          macro_name, true, n_rect->LLX(),
          n_rect->LLY(), n_rect->URX(), n_rect->URY()
      );
    }
    if (p_rect != nullptr) {
      SetWellRect(
          macro_name, false, p_rect->LLX(),
          p_rect->LLY(), p_rect->URX(), p_rect->URY()
      );
    }
  }
}

}
