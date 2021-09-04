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

    // for testing dali APIs for interact
    //dali.InstantiateIoPlacer();
    /*int count = 11;
    char *arguments[11] = {
        "place-io",
        "--auto-place",
        "--metal",
        "left",
        "m1",
        "right",
        "m1",
        "bottom",
        "m1",
        "top",
        "m1"
    };*/

    int count = 2;
    char *arguments[2] = {
        "place-io",
        "m1"
    };
    //dali.io_placer_->AutoPlaceCmd(count, arguments);
    dali.IoPinPlacement(count, arguments);
    //dali.io_placer_->AutoPlaceIoPin();

    //dali.AutoIoPinPlacement();
    dali.ExportToPhyDB();
    phy_db.WriteDef("ioplace_default_1.def");
    std::string pp_name("circuitppnp1.rect");
    phy_db.SavePpNpToRectFile(pp_name);
    std::string well_name("circuitwell1.rect");
    phy_db.SaveWellToRectFile(well_name);

    wall_time = get_wall_time() - wall_time;
    BOOST_LOG_TRIVIAL(info) << "Execution time " << wall_time << "s.\n";
    dali.Close();

    return 0;
}
