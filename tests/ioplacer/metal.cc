//
// Created by yihang on 9/9/21.
//

#include <filesystem>
#include <fstream>
#include <iostream>

#include "phydb/phydb.h"

#define SUCCESS 0
#define FAIL 1

int main() {
    std::cout << "This works!\n";

    std::string lef_file_name = "ispd19_test3.input.lef";
    std::string def_file_name = "ispd19_test3.input.def";

    std::cout << "Current path is " << std::filesystem::current_path() << '\n'; // (1)

    phydb::PhyDB phy_db;
    phy_db.ReadLef(lef_file_name);
    phy_db.ReadDef(def_file_name);

    return SUCCESS;
}