/*******************************************************************************
 *
 * Copyright (c) 2023 Yihang Yang
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
#include "filler_cell_placer.h"

#include <algorithm>
#include <fstream>
#include <string>

#include "dali/common/logging.h"

namespace dali {

/** Create filler-cell macros up to upper_width and export them to PhyDB. */
void FillerCellPlacer::CreateFillerMacros(int upper_width) {
  DaliExpects(phy_db_ptr_ != nullptr, "phydb ptr not set");

  LOG(info) << "Creating and exporting filler cells\n";
  std::string filler_lef_file_name = "dali_out_filler.lef";
  std::ofstream ost(filler_lef_file_name);
  DaliExpects(ost.is_open(), "cannot open file: " << filler_lef_file_name);

  double filler_height =
      phy_db_ptr_->tech().GetMacrosRef().begin()->GetHeight();
  for (int i = 1; i <= upper_width; ++i) {
    double width = i * ckt_ptr_->GridValueX();
    std::string filler_name = "__filler__X" + std::to_string(i) + "__";
    phydb::Macro* phydb_macro = phy_db_ptr_->AddMacro(filler_name);
    DaliExpects(phydb_macro != nullptr, "cannot add filler macro?");
    phydb_macro->SetOrigin(0, 0);
    phydb_macro->SetSize(width, filler_height);
    phydb_macro->SetClass(phydb::MacroClass::CORE_SPACER);
    phydb_macro->SetSymmetry(true, false, false);
    phy_db_ptr_->tech().AutoAddPowerGroundPin(filler_name);
    phydb_macro->ExportToFile(ost);

    ckt_ptr_->AddFillerMacro(filler_name, width, filler_height);
  }
  phy_db_ptr_->AddDummyWell();
  LOG(info) << "Filler cells exported to " << filler_lef_file_name << "\n";
}

void FillerCellPlacer::PlaceFillerCells(int lx, int ux, int ly,
                                        bool is_orient_N, int& filler_counter) {
  if (ux <= lx) {
    return;
  }

  // Keep the current policy simple and deterministic by instantiating 1X
  // fillers only. The flow already creates larger filler masters, so this can
  // be extended later to use a mixed-width strategy if reducing instance count
  // becomes important.
  Macro* filler_macro = ckt_ptr_->tech().FillerCellMacros()[0].get();
  int space = ux - lx;
  for (int i = 0; i < space; ++i) {
    std::string filler_component_name =
        "__filler_cell_component__" + std::to_string(filler_counter++);
    auto [filler_component, filler_component_id] =
        ckt_ptr_->design().FillerComponentCollection().CreateWithId(
            filler_component_name);
    filler_component.SetPlacementStatus(PLACED);
    filler_component.SetMacro(filler_macro);
    filler_component.SetId(static_cast<int>(filler_component_id));
    filler_component.SetLLX(lx + i);
    filler_component.SetLLY(ly);
    filler_component.SetOrient(is_orient_N ? N : FS);
  }
}

bool FillerCellPlacer::StartPlacement() {
  LOG(info) << "  Insert filler cells\n";
  std::vector<GeneralRow>& rows = ckt_ptr_->design().Rows();
  int filler_counter = 0;
  for (auto& row : rows) {
    for (auto& segment : row.RowSegments()) {
      segment.SortComponents();
      int lx = segment.LX();
      for (auto& component_ptr : segment.Components()) {
        int ux = component_ptr->LLX();
        PlaceFillerCells(lx, ux, row.LY(), row.IsOrientN(), filler_counter);
        lx = component_ptr->URX();
      }
      PlaceFillerCells(lx, segment.UX(), row.LY(), row.IsOrientN(),
                       filler_counter);
    }
  }

  return true;
}

}  // namespace dali
