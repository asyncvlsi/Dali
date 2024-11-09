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

#include <fstream>
#include <string>

#include "dali/common/logging.h"

namespace dali {

/****
 * Create Filler Cell BlockType and export it to PhyDB.
 * Assuming this is only for a standard cell design.
 *
 * @param upper_width: create 1X width filler cell, up to upper_widthX
 */
void FillerCellPlacer::CreateFillerCellTypes(int upper_width) {
  DaliExpects(phy_db_ptr_ != nullptr, "phydb ptr not set");

  BOOST_LOG_TRIVIAL(info) << "Creating and exporting filler cells\n";
  std::string filler_lef_file_name = "dali_out_filler.lef";
  std::ofstream ost(filler_lef_file_name);
  DaliExpects(ost.is_open(), "cannot open file: " << filler_lef_file_name);

  double filler_height =
      phy_db_ptr_->tech().GetMacrosRef().begin()->GetHeight();
  for (int i = 1; i <= upper_width; ++i) {
    double width = i * ckt_ptr_->GridValueX();
    std::string filler_name = "__filler__X" + std::to_string(i) + "__";
    phydb::Macro *phydb_macro = phy_db_ptr_->AddMacro(filler_name);
    DaliExpects(phydb_macro != nullptr, "cannot add filler cell?");
    phydb_macro->SetOrigin(0, 0);
    phydb_macro->SetSize(width, filler_height);
    phydb_macro->SetClass(phydb::MacroClass::CORE_SPACER);
    phydb_macro->SetSymmetry(true, false, false);
    phy_db_ptr_->tech().AutoAddPowerGroundPin(filler_name);
    phydb_macro->ExportToFile(ost);

    ckt_ptr_->AddFillerBlockType(filler_name, width, filler_height);
  }
  phy_db_ptr_->AddDummyWell();
  BOOST_LOG_TRIVIAL(info) << "Filler cells exported to " << filler_lef_file_name
                          << "\n";
}

void FillerCellPlacer::PlaceFillerCells(int lx, int ux, int ly,
                                        bool is_orient_N, int &filler_counter) {
  if (ux <= lx) {
    return;
  }
  auto &filler_cells = ckt_ptr_->design().Fillers();
  BlockType *filler_type_ptr = ckt_ptr_->tech().FillerCellPtrs()[0].get();
  int space = ux - lx;
  for (int i = 0; i < space; ++i) {
    std::string filler_cell_name =
        "__filler_cell_component__" + std::to_string(filler_counter++);
    Block &filler_cell =
        ckt_ptr_->design().FillerCellCollection().CreateInstance(
            filler_cell_name);
    filler_cell.SetPlacementStatus(PLACED);
    filler_cell.SetType(filler_type_ptr);
    filler_cell.SetId(
        ckt_ptr_->design().FillerCellCollection().GetInstanceIdByName(
            filler_cell_name));
    filler_cell.SetLLX(lx + i);
    filler_cell.SetLLY(ly);
    filler_cell.SetOrient(is_orient_N ? N : FS);
  }
}

bool FillerCellPlacer::StartPlacement() {
  BOOST_LOG_TRIVIAL(info) << "  Insert filler cells\n";
  std::unordered_set<int> filler_cell_widths;
  for (auto &filler : ckt_ptr_->tech().FillerCellPtrs()) {
    filler_cell_widths.insert(filler->Width());
  }
  std::vector<int> filler_widths(filler_cell_widths.begin(),
                                 filler_cell_widths.end());
  std::sort(filler_widths.begin(), filler_widths.end());

  std::vector<GeneralRow> &rows = ckt_ptr_->design().Rows();
  int filler_counter = 0;
  for (auto &row : rows) {
    for (auto &segment : row.RowSegments()) {
      segment.SortBlocks();
      int lx = segment.LX();
      for (auto &blk_ptr : segment.Blocks()) {
        int ux = blk_ptr->LLX();
        PlaceFillerCells(lx, ux, row.LY(), row.IsOrientN(), filler_counter);
        lx = blk_ptr->URX();
      }
      PlaceFillerCells(lx, segment.UX(), row.LY(), row.IsOrientN(),
                       filler_counter);
    }
  }

  return true;
}

}  // namespace dali