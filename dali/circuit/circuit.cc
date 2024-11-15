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

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <climits>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "dali/circuit/enums.h"
#include "dali/common/elapsed_time.h"
#include "dali/common/helper.h"
#include "dali/common/optregdist.h"

namespace dali {

Circuit::Circuit() { AddDummyIOPinBlockType(); }

void Circuit::InitializeFromPhyDB(phydb::PhyDB *phy_db_ptr) {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  SetPhyDB(phy_db_ptr);

  PrintHorizontalLine();
  BOOST_LOG_TRIVIAL(info) << "Load information from PhyDB\n";
  LoadTech(phy_db_ptr_);
  LoadDesign();
  LoadCell(phy_db_ptr_);
  UpdateTotalBlkArea();

  elapsed_time.RecordEndTime();
  elapsed_time.PrintTimeElapsed();
}

int Circuit::Micron2DatabaseUnit(double x) const {
  return int(std::ceil(x * DatabaseMicrons()));
}

double Circuit::DatabaseUnit2Micron(int x) const {
  return (double(x)) / DatabaseMicrons();
}

int Circuit::DistanceScaleFactorX() const {
  double d_factor_x = GridValueX() * design_.distance_microns_;
  DaliExpects(AbsResidual(d_factor_x, 1.0) < constants_.epsilon,
              "grid_value_x * distance_micron is not close to an integer?");
  return static_cast<int>(std::round(d_factor_x));
}

int Circuit::DistanceScaleFactorY() const {
  double d_factor_y = GridValueY() * design_.distance_microns_;
  DaliExpects(AbsResidual(d_factor_y, 1.0) < constants_.epsilon,
              "grid_value_y * distance_micron is not close to an integer?");
  return static_cast<int>(std::round(d_factor_y));
}

int Circuit::LocDali2PhydbX(double loc) const {
  return static_cast<int>(std::round(loc * DistanceScaleFactorX())) +
         DieAreaOffsetX();
}

int Circuit::LocDali2PhydbY(double loc) const {
  return static_cast<int>(std::round(loc * DistanceScaleFactorY())) +
         DieAreaOffsetY();
}

double Circuit::LocPhydb2DaliX(int loc) const {
  double factor_x = DistanceMicrons() * GridValueX();
  return (loc - DieAreaOffsetX()) / factor_x;
}

double Circuit::LocPhydb2DaliY(int loc) const {
  double factor_y = DistanceMicrons() * GridValueY();
  return (loc - DieAreaOffsetY()) / factor_y;
}

double Circuit::LengthPhydb2DaliX(double length) const {
  int mg_length =
      static_cast<int>(std::round(length / tech_.GetManufacturingGrid()));
  int mg_grid_x =
      static_cast<int>(std::round(GridValueX() / tech_.GetManufacturingGrid()));
  return (double)mg_length / (double)mg_grid_x;
}

double Circuit::LengthPhydb2DaliY(double length) const {
  int mg_length =
      static_cast<int>(std::round(length / tech_.GetManufacturingGrid()));
  int mg_grid_y =
      static_cast<int>(std::round(GridValueY() / tech_.GetManufacturingGrid()));
  return (double)mg_length / (double)mg_grid_y;
}

Tech &Circuit::tech() { return tech_; }

Design &Circuit::design() { return design_; }

void Circuit::SetDatabaseMicrons(int database_micron) {
  DaliExpects(
      database_micron > 0,
      "Cannot set negative database microns: Circuit::SetDatabaseMicrons()");
  tech_.database_microns_ = database_micron;
}

int Circuit::DatabaseMicrons() const { return tech_.database_microns_; }

// set manufacturing grid
void Circuit::SetManufacturingGrid(double manufacture_grid) {
  DaliExpects(manufacture_grid > 0,
              "Cannot set negative manufacturing grid: "
              "Circuit::setmanufacturing_grid_");
  tech_.manufacturing_grid_ = manufacture_grid;
}

// get manufacturing grid
double Circuit::ManufacturingGrid() const { return tech_.manufacturing_grid_; }

void Circuit::SetGridValue(double grid_value_x, double grid_value_y) {
  DaliExpects(!tech_.is_grid_set_, "once set, grid_value cannot be changed!");
  DaliExpects(grid_value_x > 0, "grid_value_x must be a positive real number!");
  DaliExpects(grid_value_y > 0, "grid_value_y must be a positive real number!");
  double residual_x = AbsResidual(grid_value_x, tech_.GetManufacturingGrid());
  DaliExpects(residual_x < constants_.epsilon,
              "grid value x is not integer multiple of manufacturing grid?");
  double residual_y = AbsResidual(grid_value_y, tech_.GetManufacturingGrid());
  DaliExpects(residual_y < constants_.epsilon,
              "grid value y is not integer multiple of manufacturing grid?");
  tech_.grid_value_x_ = grid_value_x;
  tech_.grid_value_y_ = grid_value_y;
  tech_.is_grid_set_ = true;
  SetRowHeight(grid_value_y);
}

void Circuit::SetGridFromMetalPitch() {
  DaliExpects(tech_.metal_list_.size() >= 2,
              "No enough metal layer for determining grid value in x and y! "
              "Circuit::SetGridFromMetalPitch()");
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
      "Cannot find a horizontal metal layer! Circuit::SetGridFromMetalPitch()");
  DaliExpects(
      ver_layer != nullptr,
      "Cannot find a vertical metal layer! Circuit::SetGridFromMetalPitch()");
  // BOOST_LOG_TRIVIAL(info)   << "vertical layer: " << *ver_layer->Name() << "
  // " << ver_layer->PitchX() << "\n"; BOOST_LOG_TRIVIAL(info)   << "horizontal
  // layer: " << *hor_layer->Name() << "  " << hor_layer->PitchY() << "\n";
  SetGridValue(ver_layer->PitchX(), hor_layer->PitchY());
}

double Circuit::GridValueX() const { return tech_.grid_value_x_; }

double Circuit::GridValueY() const { return tech_.grid_value_y_; }

void Circuit::SetRowHeight(double row_height) {
  DaliExpects(row_height > 0, "Setting row height to a negative value?");
  // BOOST_LOG_TRIVIAL(info) << row_height << "  " << GridValueY() << std::endl;
  double residual = AbsResidual(row_height, GridValueY());
  DaliExpects(residual < constants_.epsilon,
              "Site height is not integer multiple of grid value in Y");
  tech_.row_height_set_ = true;
  tech_.row_height_ = row_height;
}

double Circuit::RowHeightMicronUnit() const { return tech_.row_height_; }

int Circuit::RowHeightGridUnit() const {
  DaliExpects(tech_.row_height_set_,
              "Row height not set, cannot retrieve its "
              "value: Circuit::RowHeightGridUnit()\n");
  return (int)std::round(tech_.row_height_ / GridValueY());
}

std::vector<MetalLayer> &Circuit::Metals() { return tech_.metal_list_; }

std::unordered_map<std::string, int> &Circuit::MetalNameMap() {
  return tech_.metal_name_map_;
}

bool Circuit::IsMetalLayerExisting(std::string const &metal_name) {
  return tech_.metal_name_map_.find(metal_name) != tech_.metal_name_map_.end();
}

int Circuit::GetMetalLayerId(std::string const &metal_name) {
  DaliExpects(IsMetalLayerExisting(metal_name),
              "MetalLayer does not exist, cannot find it: " + metal_name);
  return tech_.metal_name_map_.find(metal_name)->second;
}

MetalLayer *Circuit::GetMetalLayerPtr(std::string const &metal_name) {
  DaliExpects(IsMetalLayerExisting(metal_name),
              "MetalLayer does not exist, cannot find it: " + metal_name);
  return &tech_.metal_list_[GetMetalLayerId(metal_name)];
}

MetalLayer *Circuit::AddMetalLayer(std::string const &metal_name, double width,
                                   double spacing, double min_area,
                                   double pitch_x, double pitch_y,
                                   MetalDirection metal_direction) {
  DaliExpects(
      !IsMetalLayerExisting(metal_name),
      "MetalLayer exist, cannot create this MetalLayer again: " + metal_name);
  int map_size = tech_.metal_name_map_.size();
  auto ret = tech_.metal_name_map_.insert(
      std::unordered_map<std::string, int>::value_type(metal_name, map_size));
  std::pair<const std::string, int> *name_id_pair_ptr = &(*ret.first);
  tech_.metal_list_.emplace_back(width, spacing, name_id_pair_ptr);
  tech_.metal_list_.back().SetMinArea(min_area);
  tech_.metal_list_.back().SetPitch(pitch_x, pitch_y);
  tech_.metal_list_.back().SetDirection(metal_direction);
  return &(tech_.metal_list_.back());
}

// report metal layer information for debugging purposes
void Circuit::ReportMetalLayers() {
  BOOST_LOG_TRIVIAL(info) << "Total MetalLayer: " << tech_.metal_list_.size()
                          << "\n";
  for (auto &metal_layer : tech_.metal_list_) {
    metal_layer.Report();
  }
  BOOST_LOG_TRIVIAL(info) << "\n";
}

std::vector<BlockType> &Circuit::BlockTypes() { return tech_.BlockTypes(); }

bool Circuit::IsBlockTypeExisting(std::string const &block_type_name) {
  return tech_.block_type_collection_.NameExists(block_type_name);
}

BlockType *Circuit::GetBlockTypePtr(std::string const &block_type_name) {
  BlockType *block_type_ptr =
      tech_.block_type_collection_.GetInstanceByName(block_type_name);
  return block_type_ptr;
}

BlockType *Circuit::AddBlockType(std::string const &block_type_name,
                                 double width, double height) {
  int gridded_width = 0;
  int gridded_height = 0;
  BlockTypeSizeMicrometerToGridValue(block_type_name, width, height,
                                     gridded_width, gridded_height);
  return AddBlockTypeWithGridUnit(block_type_name, gridded_width,
                                  gridded_height);
}

int Circuit::GetRoundOrCeilGriddedWidth(
    double width, std::string const &block_type_name) const {
  double residual = AbsResidual(width, GridValueX());
  int gridded_width = (int)std::round(width / GridValueX());
  if (residual > constants_.epsilon) {
    DaliWarning("BlockType width is not integer multiple of grid value in X: " +
                block_type_name + " rounding up");
    gridded_width = (int)std::ceil(width / GridValueX());
  }
  return gridded_width;
}

int Circuit::GetRoundOrCeilGriddedHeight(
    double height, std::string const &block_type_name) const {
  double residual = AbsResidual(height, GridValueY());
  int gridded_height = (int)std::round(height / GridValueY());
  if (residual > constants_.epsilon) {
    DaliWarning(
        "BlockType height is not integer multiple of grid value in Y: " +
        block_type_name + " rounding up");
    gridded_height = (int)std::ceil(height / GridValueY());
  }
  return gridded_height;
}

int Circuit::AddWellTapBlockType(std::string const &block_type_name,
                                 double width, double height) {
  int gridded_width = GetRoundOrCeilGriddedWidth(width, block_type_name);
  int gridded_height = GetRoundOrCeilGriddedHeight(height, block_type_name);

  return AddWellTapBlockTypeWithGridUnit(block_type_name, gridded_width,
                                         gridded_height);
}

BlockType *Circuit::AddFillerBlockType(std::string const &block_type_name,
                                       double width, double height) {
  int gridded_width = GetRoundOrCeilGriddedWidth(width, block_type_name);
  int gridded_height = GetRoundOrCeilGriddedHeight(height, block_type_name);

  return AddFillerBlockTypeWithGridUnit(block_type_name, gridded_width,
                                        gridded_height);
}

Pin *Circuit::AddBlkTypePin(BlockType *blk_type_ptr,
                            std::string const &pin_name, bool is_input) {
  DaliExpects(blk_type_ptr != nullptr, "Add a pin to a nullptr?");
  return blk_type_ptr->AddPin(pin_name, is_input);
}

void Circuit::ReportBlockType() {
  BOOST_LOG_TRIVIAL(info) << "Total BlockType: "
                          << tech_.block_type_collection_.GetSize()
                          << std::endl;
  for (auto &block_type : tech_.BlockTypes()) {
    block_type.Report();
  }
  BOOST_LOG_TRIVIAL(info) << "\n";
}

void Circuit::CopyBlockType(Circuit &circuit) {
  for (auto &block_type : circuit.tech_.BlockTypes()) {
    auto type_name = block_type.Name();
    if (type_name == "PIN") continue;
    BlockType *new_block_type = AddBlockTypeWithGridUnit(
        type_name, block_type.Width(), block_type.Height());
    for (auto &pin : block_type.PinList()) {
      new_block_type->AddPin(pin.Name(), pin.OffsetX(), pin.OffsetY());
    }
  }
}

void Circuit::SetUnitsDistanceMicrons(int distance_microns) {
  DaliExpects(distance_microns > 0, "Negative distance micron?");
  design_.distance_microns_ = distance_microns;
}

int Circuit::DistanceMicrons() const { return design_.distance_microns_; }

void Circuit::SetEnableShrinkOffGridDieArea(
    bool enable_shrink_off_grid_die_area) {
  enable_shrink_off_grid_die_area_ = enable_shrink_off_grid_die_area;
}

void Circuit::SetDieArea(int lower_x, int lower_y, int upper_x, int upper_y) {
  DaliExpects(
      GridValueX() > 0 && GridValueY() > 0,
      "Need to set positive grid values before setting placement boundary");
  DaliExpects(design_.distance_microns_ > 0,
              "Need to set def_distance_microns before setting placement "
              "boundary using Circuit::SetDieArea()");

  // TODO, extract placement boundary from rows if they are any rows
  if (enable_shrink_off_grid_die_area_) {
    RectI shrunk_die_area =
        ShrinkOffGridDieArea(lower_x, lower_y, upper_x, upper_y);
    SetBoundary(shrunk_die_area.LLX(), shrunk_die_area.LLY(),
                shrunk_die_area.URX(), shrunk_die_area.URY());
  } else {
    RectI shifted_die_area =
        ShiftOffGridDieArea(lower_x, lower_y, upper_x, upper_y);
    SetBoundary(shifted_die_area.LLX(), shifted_die_area.LLY(),
                shifted_die_area.URX(), shifted_die_area.URY());
  }
}

void Circuit::SetRectilinearDieArea(std::vector<int2d> &rectilinear_die_area) {
  design_.die_area_.distance_scale_factor_x_ = DistanceScaleFactorX();
  design_.die_area_.distance_scale_factor_y_ = DistanceScaleFactorY();
  design_.die_area_.SetRawRectilinearDieArea(rectilinear_die_area);
  design_.UpdateDieAreaPlacementBlockages();
}

int Circuit::RegionLLX() const { return design_.die_area_.region_left_; }

int Circuit::RegionURX() const { return design_.die_area_.region_right_; }

int Circuit::RegionLLY() const { return design_.die_area_.region_bottom_; }

int Circuit::RegionURY() const { return design_.die_area_.region_top_; }

int Circuit::RegionWidth() const {
  return design_.die_area_.region_right_ - design_.die_area_.region_left_;
}

int Circuit::RegionHeight() const {
  return design_.die_area_.region_top_ - design_.die_area_.region_bottom_;
}

int Circuit::DieAreaOffsetX() const {
  return design_.die_area_.die_area_offset_x_;
}

int Circuit::DieAreaOffsetY() const {
  return design_.die_area_.die_area_offset_y_;
}

void Circuit::ReserveSpaceForDesignImp(size_t components_count,
                                       size_t pins_count, size_t nets_count) {
  design_.Blocks().reserve(components_count + pins_count);
  design_.iopins_.reserve(pins_count);
  design_.nets_.reserve(nets_count);
}

std::vector<Block> &Circuit::Blocks() { return design_.Blocks(); }

bool Circuit::IsBlockExisting(std::string const &block_name) {
  return design_.block_collection_.NameExists(block_name);
}

int Circuit::GetBlockId(std::string const &block_name) {
  return design_.block_collection_.GetInstanceIdByName(block_name);
}

Block *Circuit::GetBlockPtr(std::string const &block_name) {
  return design_.block_collection_.GetInstanceByName(block_name);
}

void Circuit::AddBlock(std::string const &block_name,
                       std::string const &block_type_name, double llx,
                       double lly, PlaceStatus place_status, BlockOrient orient,
                       bool is_real_cel) {
  BlockType *block_type = GetBlockTypePtr(block_type_name);
  AddBlock(block_name, block_type, llx, lly, place_status, orient, is_real_cel);
}

void Circuit::UpdateTotalBlkArea() {
  design_.tot_white_space_ =
      (unsigned long long)(design_.die_area_.region_right_ -
                           design_.die_area_.region_left_) *
      (unsigned long long)(design_.die_area_.region_top_ -
                           design_.die_area_.region_bottom_);
  RectI die_area_bbox(
      design_.die_area_.region_left_, design_.die_area_.region_bottom_,
      design_.die_area_.region_right_, design_.die_area_.region_top_);

  design_.UpdatePlacementBlockages();

  std::vector<RectI> rects;
  for (auto &blockage : design_.PlacementBlockages()) {
    if (die_area_bbox.IsOverlap(blockage.GetRect())) {
      rects.push_back(die_area_bbox.GetOverlapRect(blockage.GetRect()));
    }
  }

  design_.total_blockage_cover_area_ = GetCoverArea(rects);
  if (design_.tot_white_space_ < design_.total_blockage_cover_area_) {
    DaliExpects(false, "Fixed blocks takes more space than available space? " +
                           std::to_string(design_.tot_blk_area_) + " " +
                           std::to_string(design_.total_blockage_cover_area_));
  }
  design_.tot_white_space_ -= design_.total_blockage_cover_area_;
  design_.tot_blk_area_ =
      design_.total_blockage_cover_area_ + design_.tot_mov_blk_area_;
}

void Circuit::ReportBlockList() {
  BOOST_LOG_TRIVIAL(info) << "Total Block: " << design_.Blocks().size() << "\n";
  for (auto &block : design_.Blocks()) {
    block.Report();
  }
  BOOST_LOG_TRIVIAL(info) << "\n";
}

void Circuit::ReportBlockMap() {
  for (auto &it : design_.BlockNameIdMap()) {
    BOOST_LOG_TRIVIAL(info) << it.first << " " << it.second << "\n";
  }
}

std::vector<IoPin> &Circuit::IoPins() { return design_.iopins_; }

bool Circuit::IsIoPinExisting(std::string const &iopin_name) {
  return !(design_.iopin_name_id_map_.find(iopin_name) ==
           design_.iopin_name_id_map_.end());
}

int Circuit::GetIoPinId(std::string const &iopin_name) {
  auto ret = design_.iopin_name_id_map_.find(iopin_name);
  if (ret == design_.iopin_name_id_map_.end()) {
    DaliExpects(false,
                "IoPin name does not exist, cannot get index " + iopin_name);
  }
  return ret->second;
}

IoPin *Circuit::GetIoPinPtr(std::string const &iopin_name) {
  return &design_.iopins_[GetIoPinId(iopin_name)];
}

IoPin *Circuit::AddIoPin(std::string const &iopin_name,
                         PlaceStatus place_status, SignalUse signal_use,
                         SignalDirection signal_direction, double lx,
                         double ly) {
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
  int iopin_x = LocPhydb2DaliX(location.x);
  int iopin_y = LocPhydb2DaliY(location.y);
  bool is_loc_set =
      (iopin.GetPlacementStatus() != phydb::PlaceStatus::UNPLACED);
  auto sig_use = SignalUse(iopin.GetUse());
  auto sig_dir = SignalDirection(iopin.GetDirection());

  if (is_loc_set) {
    IoPin *pin =
        AddIoPin(iopin_name, PLACED, sig_use, sig_dir, iopin_x, iopin_y);
    std::string layer_name = iopin.GetLayerName();
    MetalLayer *metal_layer = GetMetalLayerPtr(layer_name);
    pin->SetLayerPtr(metal_layer);
  } else {
    AddIoPin(iopin_name, UNPLACED, sig_use, sig_dir);
  }
}

void Circuit::AddPlacementBlockageFromPhyDB(phydb::Blockage &blockage) {
  auto &rects = blockage.GetRectsRef();
  DaliExpects(!rects.empty(),
              "Dali only support rectangular placement blockages now");
  for (auto &rect : rects) {
    double lx = LocPhydb2DaliX(rect.ll.x);
    double ly = LocPhydb2DaliY(rect.ll.y);
    double ux = LocPhydb2DaliX(rect.ur.x);
    double uy = LocPhydb2DaliY(rect.ur.y);
    printf("%f %f %f %f\n", lx, ly, ux, uy);
    design_.AddIntrinsicPlacementBlockage(lx, ly, ux, uy);
  }
}

void Circuit::ReportIOPin() {
  BOOST_LOG_TRIVIAL(info) << "Total IOPin: " << design_.iopins_.size() << "\n";
  for (auto &iopin : design_.iopins_) {
    iopin.Report();
  }
  BOOST_LOG_TRIVIAL(info) << "\n";
}

std::vector<Net> &Circuit::Nets() { return design_.nets_; }

bool Circuit::IsNetExisting(std::string const &net_name) {
  return !(design_.net_name_id_map_.find(net_name) ==
           design_.net_name_id_map_.end());
}

int Circuit::GetNetId(std::string const &net_name) {
  auto ret = design_.net_name_id_map_.find(net_name);
  if (ret == design_.net_name_id_map_.end()) {
    DaliExpects(false,
                "Net name does not exist, cannot find index " + net_name);
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
 * @param weight:   weight of this net, if less than 0, then default net weight
 * will be used
 * ****/
Net *Circuit::AddNet(std::string const &net_name, size_t capacity,
                     double weight) {
  DaliExpects(!IsNetExisting(net_name),
              "Net exists, cannot create this net again: " + net_name);
  int map_size = (int)design_.net_name_id_map_.size();
  design_.net_name_id_map_.insert(
      std::unordered_map<std::string, int>::value_type(net_name, map_size));
  std::pair<const std::string, int> *name_id_pair_ptr =
      &(*design_.net_name_id_map_.find(net_name));
  if (weight < 0) {
    weight = constants_.normal_net_weight;
  }
  design_.nets_.emplace_back(name_id_pair_ptr, capacity, weight);
  return &design_.nets_.back();
}

void Circuit::AddIoPinToNet(std::string const &iopin_name,
                            std::string const &net_name) {
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

void Circuit::AddBlkPinToNet(std::string const &blk_name,
                             std::string const &pin_name,
                             std::string const &net_name) {
  Block *blk_ptr = GetBlockPtr(blk_name);
  Pin *pin = blk_ptr->TypePtr()->GetPinPtr(pin_name);
  Net *net = GetNetPtr(net_name);
  net->AddBlkPinPair(blk_ptr, pin);
}

void Circuit::ReportNetList() {
  BOOST_LOG_TRIVIAL(info) << "Total Net: " << design_.nets_.size() << "\n";
  for (auto &net : design_.nets_) {
    BOOST_LOG_TRIVIAL(info)
        << "  " << net.Name() << "  " << net.Weight() << "\n";
    for (auto &block_pin : net.BlockPins()) {
      BOOST_LOG_TRIVIAL(info) << "\t" << " (" << block_pin.BlockName() << " "
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
  BOOST_LOG_TRIVIAL(info) << "Circuit brief summary:\n";
  BOOST_LOG_TRIVIAL(info) << "  movable blocks: " << TotMovBlkCnt() << "\n";
  BOOST_LOG_TRIVIAL(info) << "  fixed blocks:   " << design_.tot_fixed_blk_num_
                          << "\n";
  BOOST_LOG_TRIVIAL(info) << "  blocks:         " << TotBlkCnt() << "\n";
  BOOST_LOG_TRIVIAL(info) << "  iopins:         " << design_.iopins_.size()
                          << "\n";
  BOOST_LOG_TRIVIAL(info) << "  nets:           " << design_.nets_.size()
                          << "\n";
  BOOST_LOG_TRIVIAL(info) << "  grid size x/y:  " << GridValueX() << "/"
                          << GridValueY() << "um\n";
  BOOST_LOG_TRIVIAL(info) << "  total movable blk area: "
                          << design_.tot_mov_blk_area_ << "\n";
  BOOST_LOG_TRIVIAL(info) << "  total white space     : "
                          << design_.tot_white_space_ << "\n";
  BOOST_LOG_TRIVIAL(info) << "  total block area      : "
                          << design_.tot_blk_area_ << "\n";
  BOOST_LOG_TRIVIAL(info) << "  total space: "
                          << (long long)RegionWidth() *
                                 (long long)RegionHeight()
                          << "\n";
  BOOST_LOG_TRIVIAL(info) << "    left:   " << RegionLLX() << "\n";
  BOOST_LOG_TRIVIAL(info) << "    right:  " << RegionURX() << "\n";
  BOOST_LOG_TRIVIAL(info) << "    bottom: " << RegionLLY() << "\n";
  BOOST_LOG_TRIVIAL(info) << "    top:    " << RegionURY() << "\n";
  BOOST_LOG_TRIVIAL(info) << "  average movable width/height: "
                          << AveMovBlkWidth() << "/" << AveMovBlkHeight()
                          << "um\n";
  BOOST_LOG_TRIVIAL(info) << "  white space utility: " << WhiteSpaceUsage()
                          << "\n";
  ReportHPWL();
}

void Circuit::SetNwellParams(double width, double spacing, double op_spacing,
                             double max_plug_dist, double overhang) {
  tech_.nwell_layer_.SetParams(width, spacing, op_spacing, max_plug_dist,
                               overhang);
  tech_.n_set_ = true;
}

void Circuit::SetPwellParams(double width, double spacing, double op_spacing,
                             double max_plug_dist, double overhang) {
  tech_.pwell_layer_.SetParams(width, spacing, op_spacing, max_plug_dist,
                               overhang);
  tech_.p_set_ = true;
}

// TODO: discuss with Rajit about the necessity of the parameter ANY_SPACING in
// CELL file.
void Circuit::SetLegalizerSpacing(double same_spacing, double any_spacing) {
  tech_.same_diff_spacing_ = same_spacing;
  tech_.any_diff_spacing_ = any_spacing;
}

// TODO: discuss with Rajit about the necessity of having N/P-wells not fully
// covering the prBoundary of a given cell.
void Circuit::SetWellRect(std::string const &blk_type_name, bool is_n,
                          double lx, double ly, double ux, double uy) {
  // check if well width is smaller than max_plug_distance
  double width = ux - lx;
  double max_plug_distance = 0;
  if (is_n) {
    DaliExpects(tech_.n_set_, "Nwell layer not found, cannot set well rect: "
                                  << blk_type_name);
    max_plug_distance = tech_.nwell_layer_.MaxPlugDist();
    DaliWarns(width > max_plug_distance,
              "BlockType has a Nwell wider than max_plug_distance, this may "
              "make well legalization fail: "
                  << blk_type_name);
  } else {
    DaliExpects(tech_.p_set_, "Pwell layer not found, cannot set well rect: "
                                  << blk_type_name);
    max_plug_distance = tech_.pwell_layer_.MaxPlugDist();
    DaliWarns(width > max_plug_distance,
              "BlockType has a Pwell wider than max_plug_distance, this may "
              "make well legalization fail: "
                  << blk_type_name);
  }

  // add well rect
  BlockType *blk_type_ptr = GetBlockTypePtr(blk_type_name);
  DaliExpects(blk_type_ptr != nullptr,
              "Cannot find BlockType with name: " << blk_type_name);
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
  int lx_grid = int(std::round(lx / GridValueX()));
  int ly_grid = int(std::round(ly / GridValueY()));
  int ux_grid = int(std::round(ux / GridValueX()));
  int uy_grid = int(std::round(uy / GridValueY()));

  blk_type_ptr->AddWellRect(is_n, lx_grid, ly_grid, ux_grid, uy_grid);
}

/**
 * @brief Creates a new end-cap cell type and registers it in the circuit's
 * technology.
 *
 * This function creates a new end-cap cell type with the specified parameters
 * if it doesn't already exist. It checks for the existence of the end-cap cell
 * type in `tech_.end_cap_cell_type_map_` and throws an error if the type
 * already exists.
 *
 * @param end_cap_cell_type_name The name of the end-cap cell type to create.
 * @param width Width of the end-cap cell type.
 * @param n_well_height_in_grid_unit Height of the N-well component in grid
 * units.
 * @param p_well_height_in_grid_unit Height of the P-well component in grid
 * units.
 * @return id of the created BlockType representing the end-cap cell type.
 */
int Circuit::CreateEndCapCellType(std::string const &end_cap_cell_type_name,
                                  int width, int n_well_height_in_grid_unit,
                                  int p_well_height_in_grid_unit) {
  // Create a new BlockType for the end-cap cell type
  auto &new_end_cap_cell_type =
      tech_.end_cap_cell_type_collection_.CreateInstance(
          end_cap_cell_type_name);

  // Calculate the total height
  int height = n_well_height_in_grid_unit + p_well_height_in_grid_unit;

  // Set width height
  new_end_cap_cell_type.SetSize(width, height);

  // Report if the area exceeds INT_MAX
  if (new_end_cap_cell_type.Area() > INT_MAX) {
    new_end_cap_cell_type.Report();
  }

  // Add well info
  new_end_cap_cell_type.AddWellRect(false, 0, 0, width,
                                    p_well_height_in_grid_unit);
  new_end_cap_cell_type.AddWellRect(true, 0, p_well_height_in_grid_unit, width,
                                    height);

  return tech_.end_cap_cell_type_collection_.GetInstanceIdByName(
      end_cap_cell_type_name);
}

void Circuit::ReportWellShape() {
  for (auto &block_type : tech_.BlockTypes()) {
    if (block_type.HasWellInfo()) {
      block_type.ReportWellInfo();
    } else {
      BOOST_LOG_TRIVIAL(info)
          << "no well info for BlockType " << block_type.Name() << "\n";
    }
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
            BOOST_LOG_TRIVIAL(fatal)
                << "Expect: SPACING + Value, get: " + line << std::endl;
            exit(1);
          }
          if (legalizer_fields[0] == "SAME_DIFF_SPACING") {
            try {
              same_diff_spacing = std::stod(legalizer_fields[1]);
            } catch (...) {
              BOOST_LOG_TRIVIAL(fatal)
                  << "Invalid stod conversion: " + line << std::endl;
              exit(1);
            }
          } else if (legalizer_fields[0] == "ANY_DIFF_SPACING") {
            try {
              any_diff_spacing = std::stod(legalizer_fields[1]);
            } catch (...) {
              BOOST_LOG_TRIVIAL(fatal)
                  << "Invalid stod conversion: " + line << std::endl;
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
            BOOST_LOG_TRIVIAL(fatal)
                << "Unknow N/P well type: " + well_fields[1] << std::endl;
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
              BOOST_LOG_TRIVIAL(fatal)
                  << "Invalid stod conversion: " + well_fields[1] << std::endl;
              exit(1);
            }
          } else if (line.find("OPPOSPACING") != std::string::npos) {
            StrTokenize(line, well_fields);
            try {
              op_spacing = std::stod(well_fields[1]);
            } catch (...) {
              BOOST_LOG_TRIVIAL(fatal)
                  << "Invalid stod conversion: " + well_fields[1] << std::endl;
              exit(1);
            }
          } else if (line.find("SPACING") != std::string::npos) {
            StrTokenize(line, well_fields);
            try {
              spacing = std::stod(well_fields[1]);
            } catch (...) {
              BOOST_LOG_TRIVIAL(fatal)
                  << "Invalid stod conversion: " + well_fields[1] << std::endl;
              exit(1);
            }
          } else if (line.find("MAXPLUGDIST") != std::string::npos) {
            StrTokenize(line, well_fields);
            try {
              max_plug_dist = std::stod(well_fields[1]);
            } catch (...) {
              BOOST_LOG_TRIVIAL(fatal)
                  << "Invalid stod conversion: " + well_fields[1] << std::endl;
              exit(1);
            }
          } else if (line.find("MAXPLUGDIST") != std::string::npos) {
            StrTokenize(line, well_fields);
            try {
              overhang = std::stod(well_fields[1]);
            } catch (...) {
              BOOST_LOG_TRIVIAL(fatal)
                  << "Invalid stod conversion: " + well_fields[1] << std::endl;
              exit(1);
            }
          } else {
          }
          getline(ist, line);
        } while (line.find(end_layer_flag) == std::string::npos && !ist.eof());
        if (is_n_well) {
          SetNwellParams(width, spacing, op_spacing, max_plug_dist, overhang);
        } else {
          SetPwellParams(width, spacing, op_spacing, max_plug_dist, overhang);
        }
      }
    }

    if (line.find("MACRO") != std::string::npos) {
      std::vector<std::string> macro_fields;
      StrTokenize(line, macro_fields);
      std::string end_macro_flag = "END " + macro_fields[1];
      BlockType *p_blk_type = GetBlockTypePtr(macro_fields[1]);
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
                          << "  ly: " << ly
                          << ", grid value y: " << GridValueY() << "\n";
                    }
                    double uy_residual = AbsResidual(uy, GridValueY());
                    if (uy_residual > constants_.epsilon) {
                      BOOST_LOG_TRIVIAL(trace)
                          << "WARNING: uy of well rect for " << macro_fields[1]
                          << " is not an integer multiple of grid value y\n"
                          << "  uy: " << uy
                          << ", grid value y: " << GridValueY() << "\n";
                    }
                  } catch (...) {
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Invalid stod conversion: " + line << std::endl;
                    exit(1);
                  }
                  int lx_grid = int(std::round(lx / GridValueX()));
                  int ly_grid = int(std::round(ly / GridValueY()));
                  int ux_grid = int(std::round(ux / GridValueX()));
                  int uy_grid = int(std::round(uy / GridValueY()));
                  p_blk_type->AddWellRect(is_n, lx_grid, ly_grid, ux_grid,
                                          uy_grid);
                }
                getline(ist, line);
              } while (line.find("END REGION") == std::string::npos &&
                       !ist.eof());
            }
          } while (line.find("END REGION") == std::string::npos && !ist.eof());
        }
      } while (line.find(end_macro_flag) == std::string::npos && !ist.eof());
      // well_ptr->Report();
      p_blk_type->CheckLegality();
    }
  }
  // ReportWellShape();
}

int Circuit::MinBlkWidth() const { return design_.blk_min_width_; }

int Circuit::MaxBlkWidth() const { return design_.blk_max_width_; }

int Circuit::MinBlkHeight() const { return design_.blk_min_height_; }

int Circuit::MaxBlkHeight() const { return design_.blk_max_height_; }

unsigned long long Circuit::TotBlkArea() const { return design_.tot_blk_area_; }

int Circuit::TotBlkCnt() const { return design_.real_block_count_; }

int Circuit::TotMovBlkCnt() const { return design_.tot_mov_blk_num_; }

int Circuit::TotFixedBlkCnt() {
  // TODO: fix int type
  return static_cast<int>(design_.Blocks().size()) - design_.tot_mov_blk_num_;
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
  BOOST_LOG_TRIVIAL(info) << "  current weighted HPWL: " << WeightedHPWL()
                          << "um\n";
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
  BOOST_LOG_TRIVIAL(info) << "  current weighted bbox: "
                          << WeightedBoundingBox() << " um\n";
}

void Circuit::ReportHPWLHistogramLinear(int bin_num) {
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
  std::vector<int> count(bin_num, 0);
  for (auto &hpwl : hpwl_list) {
    int tmp_num = (int)std::floor((hpwl - min_hpwl) / step);
    if (tmp_num == bin_num) {
      tmp_num = bin_num - 1;
    }
    ++count[tmp_num];
  }

  int tot_count = design_.nets_.size();
  BOOST_LOG_TRIVIAL(info) << "\n";
  BOOST_LOG_TRIVIAL(info)
      << "                  HPWL histogram (linear scale bins)\n";
  BOOST_LOG_TRIVIAL(info) << "================================================="
                             "==================\n";
  BOOST_LOG_TRIVIAL(info) << "   HPWL interval         Count\n";
  size_t buffer_length = 1024;
  for (int i = 0; i < bin_num; ++i) {
    double lo = min_hpwl + step * i;
    double hi = lo + step;

    std::string buffer(buffer_length, '\0');
    int written_length = snprintf(&buffer[0], buffer_length,
                                  "  [%.1e, %.1e) %8d  ", lo, hi, count[i]);
    buffer.resize(written_length);

    int percent = std::ceil(50 * count[i] / (double)tot_count);
    for (int j = 0; j < percent; ++j) {
      buffer.push_back('*');
    }
    buffer.push_back('\n');
    BOOST_LOG_TRIVIAL(info) << buffer;
  }
  BOOST_LOG_TRIVIAL(info) << "================================================="
                             "==================\n";
  BOOST_LOG_TRIVIAL(info) << " * HPWL unit, grid value in X: " << GridValueX()
                          << " um\n";
  BOOST_LOG_TRIVIAL(info) << "\n";
}

/**
 * Reports the HPWL (Half Perimeter Wire Length) histogram using logarithmic
 * scale bins.
 *
 * @param bin_num The number of bins for the histogram.
 */
void Circuit::ReportHPWLHistogramLogarithm(int bin_num) {
  std::vector<double> hpwl_list;
  double min_hpwl = std::numeric_limits<double>::max();
  double max_hpwl = -std::numeric_limits<double>::max();
  hpwl_list.reserve(design_.nets_.size());
  double factor = GridValueY() / GridValueX();
  int num_nets_non_zero_hpwl = 0;

  // Calculate the HPWL for each net and store the logarithm of the HPWL values
  for (auto &net : design_.nets_) {
    double tmp_hpwl = net.WeightedHPWLX() + net.WeightedHPWLY() * factor;
    if (tmp_hpwl > 0) {
      double log_hpwl = std::log10(tmp_hpwl);
      hpwl_list.push_back(log_hpwl);
      min_hpwl = std::min(min_hpwl, log_hpwl);
      max_hpwl = std::max(max_hpwl, log_hpwl);
      ++num_nets_non_zero_hpwl;
    }
  }

  double step;
  std::vector<int> count;
  double max_diff = max_hpwl - min_hpwl;

  // If the maximum difference is too small, create a single bin
  if (max_diff < 1e-5) {
    step = 0;
    bin_num = 1;
    count.emplace_back(num_nets_non_zero_hpwl);
  } else {
    step = (max_hpwl - min_hpwl) / bin_num;
    count.assign(bin_num, 0);

    // Count the number of HPWL values falling into each bin
    for (const auto &hpwl : hpwl_list) {
      int tmp_num = static_cast<int>(std::floor((hpwl - min_hpwl) / step));
      if (tmp_num == bin_num) {
        tmp_num = bin_num - 1;
      }
      ++count[tmp_num];
    }
  }

  // Output the HPWL histogram
  BOOST_LOG_TRIVIAL(info) << "\n";
  BOOST_LOG_TRIVIAL(info)
      << "                  HPWL histogram (log scale bins)\n";
  BOOST_LOG_TRIVIAL(info) << "================================================="
                             "==================\n";
  BOOST_LOG_TRIVIAL(info) << "   HPWL interval         Count\n";

  size_t buffer_length = 1024;
  for (int i = 0; i < bin_num; ++i) {
    double lo = std::pow(10, min_hpwl + step * i);
    double hi = std::pow(10, min_hpwl + step * (i + 1));

    std::string buffer(buffer_length, '\0');
    int written_length = snprintf(&buffer[0], buffer_length,
                                  "  [%.1e, %.1e) %8d  ", lo, hi, count[i]);
    buffer.resize(written_length);

    int percent = std::ceil(50 * count[i] / design_.nets_.size());
    buffer.append(percent, '*');
    buffer.push_back('\n');
    BOOST_LOG_TRIVIAL(info) << buffer;
  }

  BOOST_LOG_TRIVIAL(info) << "================================================="
                             "==================\n";
  BOOST_LOG_TRIVIAL(info) << " * HPWL unit, grid value in X: " << GridValueX()
                          << " um\n";
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

double Circuit::HPWLCtoC() { return HPWLCtoCX() + HPWLCtoCY(); }

void Circuit::ReportHPWLCtoC() {
  BOOST_LOG_TRIVIAL(info) << "  Current HPWL: " << HPWLCtoC() << " um\n";
}

void Circuit::SaveOptimalRegionDistance(std::string const &file_name) {
  OptRegDist distance_calculator;
  distance_calculator.circuit_ = this;
  distance_calculator.SaveFile(file_name);
}

void Circuit::GenMATLABTable(std::string const &name_of_file,
                             bool only_well_tap) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

  // save place region
  SaveMatlabPatchRect(ost, RegionLLX(), RegionLLY(), RegionURX(), RegionURY(),
                      true, 1, 1, 1);
  if (!only_well_tap) {
    // save ordinary cells
    for (auto &block : design_.Blocks()) {
      SaveMatlabPatchRect(ost, block.LLX(), block.LLY(), block.URX(),
                          block.URY(), true, 0, 1, 1);
    }
  }
  // save well-tap cells
  for (auto &block : design_.WellTaps()) {
    SaveMatlabPatchRect(ost, block.LLX(), block.LLY(), block.URX(), block.URY(),
                        true, 0, 1, 1);
  }
  ost.close();
}

void Circuit::GenMATLABWellTable(std::string const &name_of_file,
                                 bool only_well_tap) {
  // generate MATLAB table for cell outlines
  std::string frame_file = name_of_file + "_outline.txt";
  GenMATLABTable(frame_file, only_well_tap);

  // generate MATLAB table for N/P-well shapes
  std::string unplug_file = name_of_file + "_unplug.txt";
  std::ofstream ost(unplug_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + unplug_file);
  if (!only_well_tap) {
    for (auto &block : design_.Blocks()) {
      block.ExportWellToMatlabPatchRect(ost);
    }
  }
  for (auto &block : design_.WellTaps()) {
    block.ExportWellToMatlabPatchRect(ost);
  }
  ost.close();
}

void Circuit::GenLongNetTable(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

  int multi_factor = 5;
  double threshold = multi_factor * AveBlkHeight();
  int count = 0;
  double ave_hpwl = 0;
  for (auto &net : design_.nets_) {
    double hpwl = net.WeightedHPWL();
    if (hpwl > threshold) {
      ave_hpwl += hpwl;
      ++count;
      for (auto &blk_pin : net.BlockPins()) {
        ost << blk_pin.AbsX() << "\t" << blk_pin.AbsY() << "\t";
      }
      ost << "\n";
    }
  }
  ave_hpwl /= count;
  BOOST_LOG_TRIVIAL(info) << "Long net report: \n"
                          << "  threshold: " << multi_factor << " " << threshold
                          << "\n"
                          << "  count:     " << count << "\n"
                          << "  ave_hpwl:  " << ave_hpwl << "\n";

  ost.close();
}

/**
 * @brief Exports the end cap cells to the given output stream.
 *
 * This function iterates through the end cap cell types in the technology
 * database, formats the relevant information, and writes it to the provided
 * output stream in the specified format. It expects a valid PhyDB pointer and
 * at least one site to be present.
 *
 * @param ost Output stream to write the end cap cell information to.
 */
void Circuit::ExportEndCapCells(std::ofstream &ost) {
  DaliExpects(phy_db_ptr_ != nullptr, "Expect PhyDB pointer to be non-null");
  auto &sites = phy_db_ptr_->GetSitesRef();
  DaliExpects(!sites.empty(), "Expect at least one site");
  std::string site_name = sites[0].GetName();

  for (auto &end_cap_cell_type :
       tech().EndCapCellTypeCollection().Instances()) {
    std::string end_cap_type = "POST";
    if (end_cap_cell_type.Name().find("pre") != std::string::npos) {
      end_cap_type = "PRE";
    }

    double width = end_cap_cell_type.Width() * GridValueX();
    double height = end_cap_cell_type.Height() * GridValueY();

    ost << "MACRO " << end_cap_cell_type.Name() << "\n";
    ost << "    CLASS ENDCAP " << end_cap_type << " ;\n";
    ost << "    FOREIGN " << end_cap_cell_type.Name() << " 0.0 0.0 ;\n";
    ost << "    ORIGIN 0.0 0.0 ;\n";
    ost << "    SIZE " << width << " BY " << height << " ;\n";
    ost << "    SYMMETRY Y ;\n";
    ost << "    SITE " << site_name << " ;\n";
    ost << "END " << end_cap_cell_type.Name() << "\n";
    ost << "\n";
  }
}

/**
 * @brief Saves a LEF file with additional end cap cell information.
 *
 * This function reads an input LEF file, copies its contents to a new output
 * LEF file, and appends end cap cell information to the output file. The new
 * output LEF file is named by appending "_with_end_cap.lef" to the provided
 * output name.
 *
 * @param input_lef_file_full_name The full name of the input LEF file.
 * @param output_lef_name The base name for the output LEF file.
 */
void Circuit::SaveLefFile(std::string const &input_lef_file_full_name,
                          std::string const &output_lef_name) {
  std::string output_lef_file_full_name = output_lef_name + "_with_end_cap.lef";
  BOOST_LOG_TRIVIAL(info) << "Writing LEF file: " << output_lef_file_full_name
                          << "\n";
  std::ofstream ost(output_lef_file_full_name.c_str());
  DaliExpects(ost.is_open(),
              "Cannot open output file " + output_lef_file_full_name);
  std::ifstream ist(input_lef_file_full_name.c_str());
  DaliExpects(ist.is_open(),
              "Cannot open input file " + input_lef_file_full_name);

  std::string line;
  // Copy all information to new LEF file
  while (std::getline(ist, line)) {
    ost << line << "\n";
  }
  ost << "\n";

  ExportEndCapCells(ost);

  ost.close();
  ist.close();
}

void Circuit::SaveCell(std::ofstream &ost, Block &blk) const {
  ost << "- " << blk.Name() << " " << blk.TypePtr()->Name() << " + "
      << blk.StatusStr() << " "
      << "( " << LocDali2PhydbX(blk.LLX()) << " " << LocDali2PhydbY(blk.LLY())
      << " ) " << OrientStr(blk.Orient()) << " ;\n";
}

void Circuit::SaveNormalCells(std::ofstream &ost,
                              std::unordered_set<PlaceStatus> *filter_out) {
  for (auto &blk : design_.Blocks()) {
    if (blk.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
    if (filter_out != nullptr &&
        filter_out->find(blk.Status()) != filter_out->end()) {
      continue;
    }
    SaveCell(ost, blk);
  }
}

void Circuit::SaveWellTapCells(std::ofstream &ost) {
  for (auto &blk : design_.WellTaps()) {
    SaveCell(ost, blk);
  }
}

void Circuit::SaveEndCapCells(std::ofstream &ost) {
  for (auto &block : design_.EndCapCellCollection().Instances()) {
    SaveCell(ost, block);
  }
}

void Circuit::SaveCircuitWellCoverCell(std::ofstream &ost,
                                       std::string const &base_name) const {
  ost << "- " << "npwells" << " " << base_name + "well" << " + "
      << "COVER "
      << "( " << LocDali2PhydbX(RegionLLX()) << " "
      << LocDali2PhydbY(RegionLLY()) << " ) "
      << "N"
      << " ;\n";
}

void Circuit::SaveCircuitPpnpCoverCell(std::ofstream &ost,
                                       std::string const &base_name) const {
  ost << "- " << "ppnps" << " " << base_name + "ppnp" << " + "
      << "COVER "
      << "( " << LocDali2PhydbX(RegionLLX()) << " "
      << LocDali2PhydbY(RegionLLY()) << " ) "
      << "N"
      << " ;\n";
}

void Circuit::ExportNormalCells(std::ofstream &ost) {
  // count the number of normal cells
  size_t cell_count = 0;
  for (auto &block : design_.Blocks()) {
    // skip dummy cells for I/O pins
    if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) {
      continue;
    }
    ++cell_count;
  }
  cell_count += design_.WellTaps().size();
  cell_count += design_.EndCapCellCollection().Instances().size();
  ost << "COMPONENTS " << cell_count << " ;\n";
  SaveNormalCells(ost);
  SaveWellTapCells(ost);
  SaveEndCapCells(ost);
  ost << "END COMPONENTS\n\n";
}

void Circuit::ExportWellTapCells(std::ofstream &ost) {
  size_t cell_count = design_.WellTaps().size();
  ost << "COMPONENTS " << cell_count << " ;\n";
  SaveWellTapCells(ost);
  ost << "END COMPONENTS\n\n";
}

void Circuit::ExportNormalAndWellTapCells(std::ofstream &ost,
                                          std::string const &base_name) {
  size_t cell_count = 0;
  for (auto &block : design_.Blocks()) {
    if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
    ++cell_count;
  }
  cell_count += design_.WellTaps().size();
  cell_count += design_.EndCapCellCollection().Instances().size();
  cell_count += 1;
  ost << "COMPONENTS " << cell_count << " ;\n";
  SaveCircuitWellCoverCell(ost, base_name);
  SaveNormalCells(ost);
  SaveWellTapCells(ost);
  SaveEndCapCells(ost);
  ost << "END COMPONENTS\n\n";
}

void Circuit::ExportNormalWellTapAndCoverCells(std::ofstream &ost,
                                               std::string const &base_name) {
  size_t cell_count = 0;
  for (auto &block : design_.Blocks()) {
    if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
    ++cell_count;
  }
  cell_count += design_.WellTaps().size();
  cell_count += design_.EndCapCellCollection().Instances().size();
  cell_count += 2;
  ost << "COMPONENTS " << cell_count << " ;\n";
  SaveCircuitWellCoverCell(ost, base_name);
  SaveCircuitPpnpCoverCell(ost, base_name);
  SaveNormalCells(ost);
  SaveWellTapCells(ost);
  SaveEndCapCells(ost);
  ost << "END COMPONENTS\n\n";
}

void Circuit::ExportCellsExcept(std::ofstream &ost,
                                std::unordered_set<PlaceStatus> *filter) {
  size_t cell_count = 0;
  for (auto &block : design_.Blocks()) {
    if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
    if (block.Status() == UNPLACED) continue;
    ++cell_count;
  }
  cell_count += design_.WellTaps().size();
  ost << "COMPONENTS " << cell_count << " ;\n";
  SaveNormalCells(ost, filter);
  SaveWellTapCells(ost);
  ost << "END COMPONENTS\n\n";
}

void Circuit::ExportCells(std::ofstream &ost, std::string const &base_name,
                          int mode) {
  switch (mode) {
    case 0: {  // no cells are saved
      ost << "COMPONENTS 0 ;\n";
      ost << "END COMPONENTS\n\n";
      break;
    }
    case 1: {  // save all normal cells, regardless of the placement status
      ExportNormalCells(ost);
      break;
    }
    case 2: {  // save only well tap cells
      ExportWellTapCells(ost);
      break;
    }
    case 3: {  // save all normal cells + dummy cell for well filling
      ExportNormalAndWellTapCells(ost, base_name);
      break;
    }
    case 4: {  // save all normal cells + dummy cell for well filling + dummy
               // cell for n/p-plus filling
      ExportNormalWellTapAndCoverCells(ost, base_name);
      break;
    }
    case 5: {  // save all placed and fixed cells
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

void Circuit::SaveIoPin(std::ofstream &ost, IoPin &iopin,
                        bool after_io_place) const {
  ost << "- " << iopin.Name() << " + NET " << iopin.NetName() << " + DIRECTION "
      << iopin.SigDirectStr() << " + USE " << iopin.SigUseStr();
  if ((after_io_place && iopin.IsPlaced()) ||
      (!after_io_place && iopin.IsPrePlaced())) {
    std::string const &metal_name = iopin.LayerName();
    ost << "\n  + LAYER " << metal_name << " ( "
        << iopin.GetShape().LLX() * design_.distance_microns_ << " "
        << iopin.GetShape().LLY() * design_.distance_microns_ << " ) "
        << " ( " << iopin.GetShape().URX() * design_.distance_microns_ << " "
        << iopin.GetShape().URY() * design_.distance_microns_ << " ) ";
    ost << "\n  + PLACED ( " << LocDali2PhydbX(iopin.X()) << " "
        << LocDali2PhydbY(iopin.Y()) << " ) ";
    if (iopin.X() == design_.die_area_.region_left_) {
      ost << "E";
    } else if (iopin.X() == design_.die_area_.region_right_) {
      ost << "W";
    } else if (iopin.Y() == design_.die_area_.region_bottom_) {
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

void Circuit::ExportIoPins(std::ofstream &ost, int mode) {
  switch (mode) {
    case 0: {  // no IOPINs are saved
      ost << "PINS 0 ;\n";
      break;
    }
    case 1: {  // save all IOPINs
      ExportIoPinsInfoAfterIoPlacement(ost);
      break;
    }
    case 2: {  // save all IOPINs with status before IO placement
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
      ost << " ( " << pin_pair.BlockName() << " " << pin_pair.PinName()
          << " ) ";
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
  for (auto &block : design_.WellTaps()) {
    ost << " ( " << block.Name() << " g0 )";
  }
  ost << "\n" << " ;\n";
  // Vdd
  ost << "- vvdddd\n";
  ost << " ";
  for (auto &block : design_.WellTaps()) {
    ost << " ( " << block.Name() << " v0 )";
  }
  ost << "\n" << " ;\n";
  ost << "END NETS\n\n";
}

void Circuit::ExportNets(std::ofstream &ost, int mode) {
  switch (mode) {
    case 0: {  // no nets are saved
      ost << "NETS 0 ;\n";
      ost << "END NETS\n\n";
      break;
    }
    case 1: {  // save all nets
      ExportAllNets(ost);
      break;
    }
    case 2: {  // save nets containing saved cells and IOPINs
      DaliExpects(false, "This part has not been implemented\n");
      break;
    }
    case 3: {  // save power nets for well tap cell
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
 *    case 4, save all normal cells + dummy cell for well filling + dummy cell
 * for n/p-plus filling case 5, save all placed cells otherwise, report an error
 * message
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
void Circuit::SaveDefFile(std::string const &base_name,
                          std::string const &name_padding,
                          std::string const &def_file_name,
                          [[maybe_unused]] int save_floorplan, int save_cell,
                          int save_iopin, int save_net) {
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

  ost.close();
  ist.close();
}

void Circuit::SaveDefFileComponent(std::string const &name_of_file,
                                   std::string const &def_file_name) {
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
      << "NumTerminals : \t\t" << Blocks().size() - design_.tot_mov_blk_num_
      << "\n";
  for (auto &block : Blocks()) {
    ost << "\t" << block.Name() << "\t"
        << block.Width() * design_.distance_microns_ * GridValueX() << "\t"
        << block.Height() * design_.distance_microns_ * GridValueY() << "\n";
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
      ost << (pair.PinPtr()->OffsetX() -
              pair.BlkPtr()->TypePtr()->Width() / 2.0) *
                 design_.distance_microns_ * GridValueX()
          << "\t"
          << (pair.PinPtr()->OffsetY() -
              pair.BlkPtr()->TypePtr()->Height() / 2.0) *
                 design_.distance_microns_ * GridValueY()
          << "\n";
    }
  }
}

void Circuit::SaveBookshelfPl(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);
  ost << "# this line is here just for ntuplace to recognize this file \n\n";
  for (auto &block : Blocks()) {
    ost << block.Name() << "\t"
        << int(block.LLX() * design_.distance_microns_ * GridValueX()) << "\t"
        << int(block.LLY() * design_.distance_microns_ * GridValueY());
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
  ost << "RowBasedPlacement :  " << name_of_file << ".nodes  " << name_of_file
      << ".nets  " << name_of_file << ".wts  " << name_of_file << ".pl  "
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
  AddWellTapBlockTypeWithGridUnit(tap_cell_name, MinBlkWidth(), MinBlkHeight());

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
  for (auto &block_type : tech_.BlockTypes()) {
    int np_edge = block_type.Height() / 2;
    block_type.AddPwellRect(0, 0, block_type.Width(), np_edge);
    block_type.AddNwellRect(0, 0, block_type.Width(), block_type.Height());
  }
}

void Circuit::SetPhyDB(phydb::PhyDB *phy_db_ptr) {
  DaliExpects(phy_db_ptr != nullptr,
              "Dali cannot initialize from a PhyDB which is a nullptr!");
  phy_db_ptr_ = phy_db_ptr;
}

BlockType *Circuit::AddBlockTypeWithGridUnit(std::string const &block_type_name,
                                             int width, int height) {
  DaliExpects(!IsBlockTypeExisting(block_type_name),
              "BlockType exist, cannot create this block type again: " +
                  block_type_name);

  BlockType &block_type =
      tech_.block_type_collection_.CreateInstance(block_type_name);
  block_type.SetSize(width, height);

  if (block_type.Area() > INT_MAX) {
    block_type.Report();
  }
  return &block_type;
}

int Circuit::AddWellTapBlockTypeWithGridUnit(std::string const &block_type_name,
                                             int width, int height) {
  AddBlockTypeWithGridUnit(block_type_name, width, height);
  int well_tap_cell_id =
      tech_.block_type_collection_.GetInstanceIdByName(block_type_name);
  tech_.well_tap_cell_type_ids_.push_back(well_tap_cell_id);
  return well_tap_cell_id;
}

BlockType *Circuit::AddFillerBlockTypeWithGridUnit(
    std::string const &block_type_name, int width, int height) {
  BlockType *filler_ptr =
      AddBlockTypeWithGridUnit(block_type_name, width, height);
  tech_.filler_ptrs_.emplace_back(filler_ptr);
  return filler_ptr;
}

void Circuit::SetBoundary(int left, int bottom, int right, int top) {
  DaliExpects(right > left, "Right boundary is not larger than Left boundary?");
  DaliExpects(top > bottom, "Top boundary is not larger than Bottom boundary?");
  design_.die_area_.region_left_ = left;
  design_.die_area_.region_right_ = right;
  design_.die_area_.region_bottom_ = bottom;
  design_.die_area_.region_top_ = top;
  design_.die_area_.die_area_set_ = true;
}

void Circuit::BlockTypeSizeMicrometerToGridValue(
    std::string const &block_type_name, double width, double height,
    int &gridded_width, int &gridded_height) {
  double residual_x = AbsResidual(width, GridValueX());
  gridded_width = -1;
  if (residual_x < constants_.epsilon) {
    gridded_width = (int)std::round(width / GridValueX());
  } else {
    gridded_width = (int)std::ceil(width / GridValueX());
    BOOST_LOG_TRIVIAL(warning)
        << "BlockType width is not integer multiple of the grid value along X: "
        << block_type_name << "\n"
        << "    width: " << width << " um\n"
        << "    grid value x: " << GridValueX() << " um\n"
        << "    residual: " << residual_x << "\n"
        << "    adjusted up to: " << gridded_width << " * " << GridValueX()
        << " um\n";
  }

  double residual_y = AbsResidual(height, GridValueY());
  gridded_height = -1;
  if (residual_y < constants_.epsilon) {
    gridded_height = (int)std::round(height / GridValueY());
  } else {
    gridded_height = (int)std::ceil(height / GridValueY());
    BOOST_LOG_TRIVIAL(warning)
        << "BlockType height is not integer multiple of the grid value along "
           "Y: "
        << block_type_name << "\n"
        << "    height: " << height << " um\n"
        << "    grid value y: " << GridValueY() << " um\n"
        << "    residual: " << residual_y << "\n"
        << "    adjusted up to: " << gridded_height << " * " << GridValueY()
        << " um\n";
  }
}

void Circuit::AddBlock(std::string const &block_name, BlockType *block_type_ptr,
                       double llx, double lly, PlaceStatus place_status,
                       BlockOrient orient, bool is_real_cel) {
  DaliExpects(design_.nets_.empty(),
              "Cannot add new Block, because net_list now is not empty");
  DaliExpects(Blocks().size() < Blocks().capacity(),
              "Cannot add new Block, because block list is full");
  DaliExpects(!IsBlockExisting(block_name),
              "Block exists, cannot create this block again: " + block_name);
  int id = static_cast<int>(design_.BlockNameIdMap().size());
  if (id < 0 || id > INT_MAX) {
    DaliExpects(false, "Cannot add more blocks, the limit is INT_MAX");
  }

  Block &block = design_.block_collection_.CreateInstance(block_name);
  block.SetType(block_type_ptr);
  block.SetId(design_.block_collection_.GetInstanceIdByName(block_name));
  block.SetLLX(llx);
  block.SetLLY(lly);
  block.SetPlacementStatus(place_status);
  block.SetOrient(orient);

  if (!is_real_cel) return;
  // update statistics of blocks
  ++design_.real_block_count_;
  design_.tot_width_ += block.Width();
  design_.tot_height_ += block.Height();
  if (block.IsMovable()) {
    ++design_.tot_mov_blk_num_;
    auto old_tot_mov_area = design_.tot_mov_blk_area_;
    design_.tot_mov_blk_area_ += block.Area();
    DaliExpects(old_tot_mov_area <= design_.tot_mov_blk_area_,
                "Total Movable Block Area Overflow, choose a different "
                "MANUFACTURINGGRID/unit");
    design_.tot_mov_width_ += block.Width();
    design_.tot_mov_height_ += block.Height();
  } else {
    ++design_.tot_fixed_blk_num_;
    design_.AddFixedCellPlacementBlockage(block);
  }
  if (block.Height() < design_.blk_min_height_) {
    design_.blk_min_height_ = block.Height();
  }
  if (block.Height() > design_.blk_max_height_) {
    design_.blk_max_height_ = block.Height();
  }
  if (block.Width() < design_.blk_min_width_) {
    design_.blk_min_width_ = block.Width();
  }
  if (block.Width() > design_.blk_min_width_) {
    design_.blk_max_width_ = block.Width();
  }
}

/****
 * This member function adds a dummy BlockType for IOPINs.
 * The name of this dummy BlockType is "PIN", and it contains one cell pin with
 * name "pin". The size of "PIN" BlockType is 0 (width) and 0(height). The
 * relative location of the only cell pin "pin" is (0,0) with size 0.
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
  int map_size = design_.iopin_name_id_map_.size();
  auto ret = design_.iopin_name_id_map_.insert(
      std::unordered_map<std::string, int>::value_type(iopin_name, map_size));
  std::pair<const std::string, int> *name_id_pair_ptr = &(*ret.first);
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
IoPin *Circuit::AddPlacedIOPin(std::string const &iopin_name, double lx,
                               double ly) {
  DaliExpects(design_.nets_.empty(),
              "Cannot add new IOPIN, because net_list now is not empty");
  DaliExpects(!IsIoPinExisting(iopin_name),
              "IOPin exists, cannot create this IOPin again: " + iopin_name);
  int map_size = (int)design_.iopin_name_id_map_.size();
  auto ret = design_.iopin_name_id_map_.insert(
      std::unordered_map<std::string, int>::value_type(iopin_name, map_size));
  std::pair<const std::string, int> *name_id_pair_ptr = &(*ret.first);
  design_.iopins_.emplace_back(name_id_pair_ptr, lx, ly);
  design_.pre_placed_io_count_ += 1;

  // add a dummy cell corresponding to this IOPIN to block_list.
  AddBlock(iopin_name, tech_.io_dummy_blk_type_ptr_, lx, ly, FIXED, N, false);

  return &(design_.iopins_.back());
}

RectI Circuit::ShrinkOffGridDieArea(int lower_x, int lower_y, int upper_x,
                                    int upper_y) {
  int factor_x = DistanceScaleFactorX();
  int factor_y = DistanceScaleFactorY();

  int left = 0;
  double f_left = lower_x / static_cast<double>(factor_x);
  if (AbsResidual(f_left, 1) > 1e-5) {
    left = std::ceil(f_left);
    BOOST_LOG_TRIVIAL(info)
        << "left placement boundary is not on placement grid: \n"
        << "  shrink left from " << lower_x << " to " << left * factor_x
        << "\n";
  } else {
    left = static_cast<int>(std::round(f_left));
  }

  int right = 0;
  double f_right = upper_x / static_cast<double>(factor_x);
  if (AbsResidual(f_right, 1) > 1e-5) {
    right = std::floor(f_right);
    BOOST_LOG_TRIVIAL(info)
        << "right placement boundary is not on placement grid: \n"
        << "  shrink right from " << upper_x << " to " << right * factor_x
        << "\n";
  } else {
    right = static_cast<int>(std::round(f_right));
  }

  int bottom = 0;
  double f_bottom = lower_y / static_cast<double>(factor_y);
  if (AbsResidual(f_bottom, 1) > 1e-5) {
    bottom = std::ceil(f_bottom);
    BOOST_LOG_TRIVIAL(info)
        << "bottom placement boundary is not on placement grid: \n"
        << "  shrink bottom from " << lower_y << " to " << bottom * factor_y
        << "\n";
  } else {
    bottom = static_cast<int>(std::round(f_bottom));
  }

  int top = 0;
  double f_top = upper_y / static_cast<double>(factor_y);
  if (AbsResidual(f_top, 1) > 1e-5) {
    top = std::floor(f_top);
    BOOST_LOG_TRIVIAL(info)
        << "top placement boundary is not on placement grid: \n"
        << "  shrink top from " << upper_y << " to " << top * factor_y << "\n";
  } else {
    top = static_cast<int>(std::round(f_top));
  }

  design_.die_area_.die_area_offset_x_ = 0;
  design_.die_area_.die_area_offset_y_ = 0;
  design_.die_area_.die_area_offset_x_residual_ = 0;
  design_.die_area_.die_area_offset_y_residual_ = 0;

  return {left, bottom, right, top};
}

RectI Circuit::ShiftOffGridDieArea(int lower_x, int lower_y, int upper_x,
                                   int upper_y) {
  int factor_x = DistanceScaleFactorX();
  int factor_y = DistanceScaleFactorY();

  design_.die_area_.die_area_offset_x_ = lower_x % factor_x;
  design_.die_area_.die_area_offset_y_ = lower_y % factor_y;
  int adjusted_lower_x = lower_x - design_.die_area_.die_area_offset_x_;
  int adjusted_lower_y = lower_y - design_.die_area_.die_area_offset_y_;
  int adjusted_upper_x = upper_x - design_.die_area_.die_area_offset_x_;
  int adjusted_upper_y = upper_y - design_.die_area_.die_area_offset_y_;
  if (design_.die_area_.die_area_offset_x_ != 0) {
    BOOST_LOG_TRIVIAL(info)
        << "left placement boundary is not on placement grid: \n"
        << "  shift left from " << lower_x << " to " << adjusted_lower_x << "\n"
        << "  shift right from " << upper_x << " to " << adjusted_upper_x
        << "\n";
  }
  if (design_.die_area_.die_area_offset_y_ != 0) {
    BOOST_LOG_TRIVIAL(info)
        << "bottom placement boundary is not on placement grid: \n"
        << "  shift bottom from " << lower_y << " to " << adjusted_lower_y
        << "\n"
        << "  shift top from " << upper_y << " to " << adjusted_upper_y << "\n";
  }
  lower_x = adjusted_lower_x;
  lower_y = adjusted_lower_y;
  upper_x = adjusted_upper_x;
  upper_y = adjusted_upper_y;
  int left = lower_x / factor_x;
  int bottom = lower_y / factor_y;

  design_.die_area_.die_area_offset_x_residual_ = upper_x % factor_x;
  design_.die_area_.die_area_offset_y_residual_ = upper_y % factor_y;
  adjusted_upper_x = upper_x - design_.die_area_.die_area_offset_x_residual_;
  adjusted_upper_y = upper_y - design_.die_area_.die_area_offset_y_residual_;
  if (design_.die_area_.die_area_offset_x_residual_ != 0) {
    BOOST_LOG_TRIVIAL(info)
        << "right placement boundary is not on placement grid: \n"
        << "  shrink right from " << upper_x << " to " << adjusted_upper_x
        << "\n";
  }
  if (design_.die_area_.die_area_offset_y_residual_ != 0) {
    BOOST_LOG_TRIVIAL(info)
        << "top placement boundary is not on placement grid: \n"
        << "  shrink top from " << upper_y << " to " << adjusted_upper_y
        << "\n";
  }
  upper_x = adjusted_upper_x;
  upper_y = adjusted_upper_y;
  int right = upper_x / factor_x;
  int top = upper_y / factor_y;

  return {left, bottom, right, top};
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
  bool is_placement_grid_set =
      phy_db_tech.GetPlacementGrids(grid_value_x, grid_value_y);
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
          "MinWidth and Width not found in PhyDB for layer: " + layer_name);

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
          "Dali expects a non-negative MinArea for layer: " + layer_name);

      auto direction = MetalDirection(layer.GetDirection());
      AddMetalLayer(layer_name, min_width, min_spacing, min_area, pitch_x,
                    pitch_y, direction);
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
      int block_index = AddWellTapBlockType(macro_name, width, height);
      blk_type = &(tech_.BlockTypes()[block_index]);
    } else if (macro.GetClass() == phydb::MacroClass::CORE_SPACER) {
      blk_type = AddFillerBlockType(macro_name, width, height);
    } else if (macro.GetClass() == phydb::MacroClass::ENDCAP_PRE) {
      blk_type = AddBlockType(macro_name, width, height);
      tech_.pre_end_cap_cell_ptr_ = blk_type;
    } else if (macro.GetClass() == phydb::MacroClass::ENDCAP_POST) {
      blk_type = AddBlockType(macro_name, width, height);
      tech_.post_end_cap_cell_ptr_ = blk_type;
    } else {
      blk_type = AddBlockType(macro_name, width, height);
    }
    auto &macro_pins = macro.GetPinsRef();
    for (auto &pin : macro_pins) {
      std::string pin_name(pin.GetName());
      // if (pin_name == "Vdd" || pin_name == "GND") continue;

      bool is_input = true;
      auto pin_direction = pin.GetDirection();
      is_input = (pin_direction == phydb::SignalDirection::INPUT);
      Pin *new_pin = blk_type->AddPin(pin_name, is_input);

      auto &layer_rects = pin.GetLayerRectRef();
      DaliExpects(!layer_rects.empty(),
                  "No physical pins, Macro: " << blk_type->Name()
                                              << ", pin: " << pin_name);

      auto bbox = pin.GetBoundingBox();
      double llx = LengthPhydb2DaliX(bbox.LLX());
      double urx = LengthPhydb2DaliX(bbox.URX());
      double lly = LengthPhydb2DaliY(bbox.LLY());
      double ury = LengthPhydb2DaliY(bbox.URY());
      new_pin->SetOffset((llx + urx) / 2.0, (lly + ury) / 2.0);
      double bbox_width = LengthPhydb2DaliX(bbox.GetWidth());
      double bbox_height = LengthPhydb2DaliY(bbox.GetHeight());
      new_pin->SetBoundingBoxSize(bbox_width, bbox_height);
    }
  }
  tech_.block_type_collection_.Freeze();
}

void Circuit::ReserveSpaceForDesign() {
  auto &phy_db_design = *(phy_db_ptr_->GetDesignPtr());
  auto &components = phy_db_design.GetComponentsRef();
  int components_size = (int)components.size();

  auto &iopins = phy_db_design.GetIoPinsRef();
  int pins_size = (int)iopins.size();

  auto &nets = phy_db_design.GetNetsRef();
  int nets_size = (int)nets.size();

  ReserveSpaceForDesignImp(components_size, pins_size, nets_size);
}

void Circuit::LoadUnits() {
  auto &phy_db_design = *(phy_db_ptr_->GetDesignPtr());
  SetUnitsDistanceMicrons(phy_db_design.GetUnitsDistanceMicrons());
}

void Circuit::LoadDieArea() {
  auto rectilinear_polygon_die_area =
      phy_db_ptr_->RectilinearPolygonDieAreaRef();
  std::vector<int2d> polygon_die_area;
  polygon_die_area.reserve(rectilinear_polygon_die_area.size());
  for (auto &point : rectilinear_polygon_die_area) {
    polygon_die_area.emplace_back(point.x, point.y);
  }
  SetRectilinearDieArea(polygon_die_area);
}

void Circuit::LoadComponents() {
  auto &phy_db_design = *(phy_db_ptr_->GetDesignPtr());
  auto &components = phy_db_design.GetComponentsRef();
  for (auto &comp : components) {
    std::string blk_name(comp.GetName());
    std::string blk_type_name(comp.GetMacro()->GetName());
    auto location = comp.GetLocation();
    int llx = location.x;
    int lly = location.y;
    double lx = std::round(LocPhydb2DaliX(llx));
    double ly = std::round(LocPhydb2DaliY(lly));
    auto place_status = PlaceStatus(comp.GetPlacementStatus());
    auto orient = BlockOrient(comp.GetOrientation());
    AddBlock(blk_name, blk_type_name, lx, ly, place_status, orient);
  }
}

void Circuit::LoadIoPins() {
  auto &phy_db_design = *(phy_db_ptr_->GetDesignPtr());
  auto &io_pins = phy_db_design.GetIoPinsRef();
  for (auto &io_pin : io_pins) {
    AddIoPinFromPhyDB(io_pin);
  }
}

void Circuit::LoadPlacementBlockages() {
  auto &phy_db_design = *(phy_db_ptr_->GetDesignPtr());
  auto &blockages = phy_db_design.GetBlockagesRef();
  for (auto &blockage : blockages) {
    if (blockage.IsPlacement()) {
      AddPlacementBlockageFromPhyDB(blockage);
    }
  }
}

void Circuit::LoadNets() {
  auto &phy_db_design = *(phy_db_ptr_->GetDesignPtr());
  auto &components = phy_db_design.GetComponentsRef();
  auto &io_pins = phy_db_design.GetIoPinsRef();
  auto &nets = phy_db_design.GetNetsRef();
  for (auto &net : nets) {
    std::string net_name(net.GetName());
    auto &net_pins = net.GetPinsRef();
    std::vector<int> &io_pin_ids = net.GetIoPinIdsRef();
    int net_capacity = int(net_pins.size() + io_pin_ids.size());
    AddNet(net_name, net_capacity, design_.normal_signal_weight_);

    for (int &id : io_pin_ids) {
      AddIoPinToNet(io_pins[id].GetName(), net_name);
    }
    int sz = (int)net_pins.size();
    for (int i = 0; i < sz; ++i) {
      int comp_id = net_pins[i].InstanceId();
      std::string const &comp_name = components[comp_id].GetName();
      int pin_id = net_pins[i].PinId();
      std::string const &pin_name = components[comp_id].GetPinName(pin_id);
      AddBlkPinToNet(comp_name, pin_name, net_name);
    }
  }
}

void Circuit::LoadDesign() {
  ReserveSpaceForDesign();
  LoadUnits();
  LoadDieArea();
  LoadComponents();
  LoadIoPins();
  LoadPlacementBlockages();
  LoadNets();

  design().BlockCollection().Freeze();
}

void Circuit::LoadCell(phydb::PhyDB *phy_db_ptr) {
  auto &phy_db_tech = *(phy_db_ptr->GetTechPtr());
  if (!phy_db_tech.IsWellInfoSet()) {
    BOOST_LOG_TRIVIAL(info) << "N/P-Well layer info not found in PhyDB\n";
    BOOST_LOG_TRIVIAL(info) << "Will come up with some fake info\n";
  }

  double same_diff_spacing = 0, any_diff_spacing = 0;
  phy_db_tech.GetDiffWellSpacing(same_diff_spacing, any_diff_spacing);
  SetLegalizerSpacing(same_diff_spacing, any_diff_spacing);

  tech_.pre_end_cap_min_width_ = phy_db_tech.GetPreEndCapMinWidth();
  tech_.pre_end_cap_min_p_height_ = phy_db_tech.GetPreEndCapMinPHeight();
  tech_.pre_end_cap_min_n_height_ = phy_db_tech.GetPreEndCapMinNHeight();
  tech_.post_end_cap_min_width_ = phy_db_tech.GetPreEndCapMinWidth();
  tech_.post_end_cap_min_p_height_ = phy_db_tech.GetPostEndCapMinPHeight();
  tech_.post_end_cap_min_n_height_ = phy_db_tech.GetPostEndCapMinNHeight();

  auto *n_well_layer = phy_db_tech.GetNwellLayerPtr();
  if (n_well_layer != nullptr) {
    double width = n_well_layer->GetWidth();
    double spacing = n_well_layer->GetSpacing();
    double op_spacing = n_well_layer->GetOpSpacing();
    double max_plug_dist = n_well_layer->GetMaxPlugDist();
    double overhang = n_well_layer->GetOverhang();
    SetNwellParams(width, spacing, op_spacing, max_plug_dist, overhang);
  } else {
    BOOST_LOG_TRIVIAL(info)
        << "No N-well layer info provided, creating fake info\n";
    SetNwellParams(0.0, 0.0, 0.0, 1e8, 0.0);
  }

  auto *p_well_layer = phy_db_tech.GetPwellLayerPtr();
  if (p_well_layer != nullptr) {
    double width = p_well_layer->GetWidth();
    double spacing = p_well_layer->GetSpacing();
    double op_spacing = p_well_layer->GetOpSpacing();
    double max_plug_dist = p_well_layer->GetMaxPlugDist();
    double overhang = p_well_layer->GetOverhang();
    SetPwellParams(width, spacing, op_spacing, max_plug_dist, overhang);
  } else {
    BOOST_LOG_TRIVIAL(info)
        << "No P-well layer info provided, creating fake info\n";
    SetPwellParams(0.0, 0.0, 0.0, 1e8, 0.0);
  }

  for (auto &macro : phy_db_tech.GetMacrosRef()) {
    std::string macro_name(macro.GetName());
    auto &macro_well = macro.WellPtrRef();

    if (macro_well != nullptr) {
      auto *n_rect = macro_well->GetNwellRectPtr();
      auto *p_rect = macro_well->GetPwellRectPtr();
      if (n_rect == nullptr && p_rect == nullptr) {
        DaliExpects(
            false, "N/P-well geometries not provided for MACRO: " + macro_name);
      }
      if (n_rect != nullptr) {
        SetWellRect(macro_name, true, n_rect->LLX(), n_rect->LLY(),
                    n_rect->URX(), n_rect->URY());
      }
      if (p_rect != nullptr) {
        SetWellRect(macro_name, false, p_rect->LLX(), p_rect->LLY(),
                    p_rect->URX(), p_rect->URY());
      }
    } else {
      BOOST_LOG_TRIVIAL(info)
          << "No well info provided for MACRO: " + macro_name << "\n";
      BOOST_LOG_TRIVIAL(info)
          << "Creating fake well info for MACRO: " + macro_name << "\n";
      double height = macro.GetHeight();
      double width = macro.GetWidth();
      SetWellRect(macro_name, false, 0, 0, width, height / 2.0);
      SetWellRect(macro_name, true, 0, height / 2.0, width, height);
    }
  }
}

}  // namespace dali
