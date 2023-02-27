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

/**** the following is an example to add a BlockType with one cell pin to a Circuit
 * std::string blk_type_name = "nand2"; // block type with name nand2
 * int width = 4; // width 4
 * int height = 8; // height 8
 * BlockType *blk_type_ptr = circuit.AddBlockType(blk_type_name, width, height); // add this BlockType to circuit
 * std::string pin_name = "in"; // block pin with name "in"
 * Pin *pin_ptr = circuit.AddBlkTypePin(blk_type_ptr, pin_name); // add this Pin to this BlockType
 * circuit.AddBlkTypePinRect(pin_ptr, 0, 1.9, 0.2, 2.1); // add the shape of this Pin
 ****/

#include "dali/circuit/circuit.h"

using namespace dali;

// TODO: move this to the tests/ folder

int main() {
  InitLogging();
  BOOST_LOG_TRIVIAL(info) << "Example of creating a Circuit instance\n";

  // create a blank circuit
  Circuit circuit;

  // set database micron and manufacturing grid
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(0.001);

  // set metal layer if you want to do IO Placement and use metal pitches as grid value
  std::string metal_layer_name;
  metal_layer_name = "m1";
  circuit.AddMetalLayer(metal_layer_name, 0.1, 0.1, 0.042, 0.2, 0.2, VERTICAL);
  metal_layer_name = "m2";
  circuit.AddMetalLayer(
      metal_layer_name,
      0.1,
      0.1,
      0.042,
      0.2,
      0.2,
      HORIZONTAL
  );

  // set grid value and row height
  circuit.SetGridValue(0.2, 0.2);
  circuit.SetRowHeight(0.2);

  // add block type "NAND2"
  std::string inv_type_name = "INV2";
  double inv_width = 0.8;
  double inv_height = 1.6;
  BlockType
      *inv_ptr = circuit.AddBlockType(inv_type_name, inv_width, inv_height);

  // add cell pins
  std::string in_name = "IN";
  double in_lx = 0.0;
  double in_ly = 0.7;
  double in_ux = 0.2;
  double in_uy = 0.9;
  Pin *in_pin_ptr = circuit.AddBlkTypePin(inv_ptr, in_name, true);
  in_pin_ptr->SetOffset((in_lx + in_ux) / 2.0, (in_ly + in_uy) / 2.0);

  std::string out_name = "OUT";
  double out_lx = 0.6;
  double out_ly = 0.7;
  double out_ux = 0.8;
  double out_uy = 0.9;
  Pin *out_pin_ptr = circuit.AddBlkTypePin(inv_ptr, out_name, false);
  out_pin_ptr->SetOffset((out_lx + out_ly) / 2.0, (out_ux + out_uy) / 2.0);

  // set DEF Units distance microns, which would be better the same as LEF database micron.
  circuit.SetUnitsDistanceMicrons(1000);

  // set die area
  int die_left = 0.0 * circuit.DistanceMicrons();
  int die_bottom = 0.0 * circuit.DistanceMicrons();
  int die_right = 10.0 * circuit.DistanceMicrons();
  int die_top = 10.0 * circuit.DistanceMicrons();
  circuit.SetDieArea(die_left, die_bottom, die_right, die_top);

  // specify how many instances there are.
  circuit.ReserveSpaceForDesignImp(2, 2, 3);

  // add block instances
  std::string inv1_name = "inv1";
  circuit.AddBlock(inv1_name, inv_type_name, 0, 0, UNPLACED, N, true);
  std::string inv2_name = "inv2";
  circuit.AddBlock(inv2_name, inv_type_name, 0, 0, UNPLACED, N, true);

  // add IOPIN
  std::string chip_in = "io_in";
  circuit.AddIoPin(chip_in, UNPLACED, SIGNAL, INPUT, 0, 0);
  std::string chip_out = "io_out";
  circuit.AddIoPin(chip_out, UNPLACED, SIGNAL, OUTPUT, 0, 0);

  // add net
  std::string net_in_name = "net_in";
  circuit.AddNet(net_in_name, 2);
  circuit.AddIoPinToNet(chip_in, net_in_name);
  circuit.AddBlkPinToNet(inv1_name, in_name, net_in_name);

  std::string net_between_name = "net_between";
  circuit.AddNet(net_between_name, 2);
  circuit.AddBlkPinToNet(inv1_name, out_name, net_between_name);
  circuit.AddBlkPinToNet(inv2_name, in_name, net_between_name);

  std::string net_out_name = "net_out";
  circuit.AddNet(net_out_name, 2);
  circuit.AddBlkPinToNet(inv2_name, out_name, net_out_name);
  circuit.AddIoPinToNet(chip_out, net_out_name);

  // Report circuit detail
  circuit.ReportMetalLayers();
  circuit.ReportBlockType();
  circuit.ReportBlockList();
  circuit.ReportIOPin();
  circuit.ReportNetList();

  return 0;
}
