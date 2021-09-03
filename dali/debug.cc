//
// Created by Yihang Yang on 5/14/19.
//

#include <ctime>
#include <unistd.h>

#include <iostream>
#include <vector>

#include "dali.h"

#define TEST_LG 0

using namespace dali;

int main() {
    double wall_time = get_wall_time();

    std::string lef_file_name = "processor.lef";
    std::string def_file_name = "processor.def";
    std::string cell_file_name = "processor.cell";

    // read LEF/DEF/CELL
    phydb::PhyDB phy_db;
    //phy_db.SetPlacementGrids(0.01, 0.01);
    phy_db.ReadLef(lef_file_name);
    phy_db.ReadDef(def_file_name);
    phy_db.ReadCell(cell_file_name);

    // initialize Dali
    Dali dali(&phy_db, boost::log::trivial::info);

    //phydb::Macro *cell = phy_db.GetMacroPtr("WELLTAPX1");
    //dali.AddWellTaps(cell, 60, true);
    dali.StartPlacement(0.65);
    //dali.GlobalPlace(1.00);
    //dali.ExternalDetailedPlaceAndLegalize("innovus");
    //dali.SimpleIoPinPlacement("m1");
    //dali.ExportToDEF(def_file_name);

    dali.AutoIoPinPlacement();
    dali.ExportToPhyDB();
    phy_db.WriteDef("ioplace_default_1.def");

    wall_time = get_wall_time() - wall_time;
    BOOST_LOG_TRIVIAL(info) << "Execution time " << wall_time << "s.\n";
    dali.Close();

    return 0;
}
