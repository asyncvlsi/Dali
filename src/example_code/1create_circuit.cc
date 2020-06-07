//
// Created by Yihang Yang on 2020-06-07.
//

/**** the following is an example to add a BlockType with one cell pin to a Circuit
 * std::string blk_type_name = "nand2"; // block type with name nand2
 * int width = 4; // width 4
 * int height = 8; // height 8
 * BlockType *blk_type_ptr = circuit.AddBlockType(blk_type_name, width, height); // add this BlockType to circuit
 * std::string pin_name = "in"; // block pin with name "in"
 * Pin *pin_ptr = circuit.AddBlkTypePin(blk_type_ptr, pin_name); // add this Pin to this BlockType
 * circuit.AddBlkTypePinRect(pin_ptr, 0, 1.9, 0.2, 2.1); // add the shape of this Pin
 ****/

#include "circuit.h"

int main() {
  std::cout << "Example of creating a Circuit instance\n";

  // create a blank circuit
  Circuit circuit;

  // set database micron and manufacturing grid
  circuit.setDatabaseMicron(1000);
  circuit.setManufacturingGrid(0.001);

  // set metal layer if you want to do IO Placement and use metal pitches as grid value
  std::string metal_layer_name;
  metal_layer_name = "m1";
  circuit.AddMetalLayer(metal_layer_name, 0.1, 0.1, 0.042, 0.2, 0.2, VERTICAL_);
  metal_layer_name = "m2";
  circuit.AddMetalLayer(metal_layer_name, 0.1, 0.1, 0.042, 0.2, 0.2, HORIZONTAL_);

  // set grid value and row height
  circuit.setGridValue(0.2, 0.2);
  circuit.setRowHeightMicron(0.2);

  //
}