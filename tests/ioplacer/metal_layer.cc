//
// Created by Yihang Yang on 9/9/21.
//

#include <fstream>
#include <iostream>

#include "phydb/phydb.h"

#include "dali/dali.h"
#include "helper.h"

#define SUCCESS 0
#define FAIL 1
#define NUM_OF_ARGS 2

using namespace dali;

/****
 * @brief Testcase for IoPlacement command "place-io metal_layer".
 *
 * This is a simple testcase to show that using command
 *     "place-io m1"
 * we can obtain a placement result satisfying:
 * 1. all IOPINs are placed on placement boundary
 * 2. IOPINs does not overlap with each other
 * 3. Physical geometries of all IOPINs are on metal layer "m1"
 * 4. All locations respect manufacturing grid specified in LEF
 * 5. Physical geometries of all IOPINs are inside the placement region
 *
 * @return 0 if this test is passed, 1 if failed
 */
int main() {
    std::string lef_file_name = "ispd19_test3.input.lef";
    std::string def_file_name = "ispd19_test3.input.def";

    // initialie PhyDB
    auto *phy_db = new phydb::PhyDB;
    phy_db->ReadLef(lef_file_name);
    phy_db->ReadDef(def_file_name);

    // un-place all iopins
    SetAllIoPinsToUnplaced(phy_db);

    // initialize Dali
    Dali dali(
        phy_db,
        boost::log::trivial::info,
        ""
    );

    // perform IO placement
    int arg_count = NUM_OF_ARGS;
    char *arg_values[NUM_OF_ARGS] = {
        (char *) "place-io",
        (char *) "Metal1"
    };
    bool is_ioplace_success = dali.IoPinPlacement(arg_count, arg_values);
    if (!is_ioplace_success) {
        return FAIL;
    }

    // export the result to PhyDB and save the result to a DEF file
    dali.ExportToPhyDB();
    std::string out_def_file_name = "metal_layer.def";
    phy_db->WriteDef(out_def_file_name);
    delete phy_db;

    // read the DEF file back to the memory, and perform checking
    phy_db = new phydb::PhyDB;
    phy_db->ReadLef(lef_file_name);
    phy_db->ReadDef(out_def_file_name);

    bool is_legal;
    is_legal = IsEveryIoPinPlacedOnBoundary(phy_db);
    if (!is_legal) return FAIL;

    is_legal = IsNoIoPinOverlap(phy_db);
    if (!is_legal) return FAIL;

    return SUCCESS;
}
