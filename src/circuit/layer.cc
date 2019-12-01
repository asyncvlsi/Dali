//
// Created by Yihang Yang on 11/28/19.
//

#include "layer.h"


void MetalLayer::Report() {
  std::cout << "  MetalLayer Name: " << *Name() << "\n"
            << "    Assigned Num: " << Num() << "\n"
            << "    Width and Spacing: " << Width() << " " << Spacing() << "\n"
            << "    MinArea: " << Area() << "\n"
            << "    Direction: " << MetalDirectionStr(Direction()) << "\n"
            << "    Pitch: " << PitchX() << "  " << PitchY() << "\n";
}