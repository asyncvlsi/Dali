//
// Created by yihang on 9/9/21.
//

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "phydb/phydb.h"

#include "dali/dali.h"
#include "helper.h"

#define SUCCESS 0
#define FAIL 1
#define NUM_OF_ARGS 2

using namespace dali;

int main() {
    std::string lef_file_name = "ispd19_test3.input.lef";
    std::string def_file_name = "ispd19_test3.input.def";

    // initialie PhyDB
    phydb::PhyDB phy_db;
    phy_db.ReadLef(lef_file_name);
    phy_db.ReadDef(def_file_name);

    SetAllIoPinsToUnplaced(phy_db);

    // initialize Dali
    Dali dali(&phy_db, boost::log::trivial::info);

    int arg_count = NUM_OF_ARGS;
    char *arg_values[NUM_OF_ARGS] = {
        (char *) "place-io",
        (char *) "m1"
    };

    //dali.IoPinPlacement(arg_count, arg_values);
    //dali.ExportToPhyDB();
    //phy_db.WriteDef("ioplace_default_1.def");

    return SUCCESS;
}