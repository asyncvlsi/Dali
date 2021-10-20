//
// Created by Yihang Yang on 9/24/19.
//

/****
 * This is a stand-alone hpwl calculator, it will report:
 * 1. HPWL
 *    pin-to-pin
 *    center-to-center
 * 2. Weighted HPWL, a weight file needs to be provided
 *    pin-to-pin
 *    center-to-center
 * ****/
//TODO: fix this
#include <iostream>

#include "dali/circuit/circuit.h"

void ReportUsage();

using namespace dali;

int main(int argc, char *argv[]) {
    if (argc != 5) {
        ReportUsage();
        return 1;
    }
    InitLogging();
    std::string lef_file_name;
    std::string def_file_name;

    for (int i = 1; i < argc;) {
        std::string arg(argv[i++]);
        if (arg == "-lef" && i < argc) {
            lef_file_name = std::string(argv[i++]);
        } else if (arg == "-def" && i < argc) {
            def_file_name = std::string(argv[i++]);
        } else {
            BOOST_LOG_TRIVIAL(info) << "Unknown command line option: "
                                    << argv[i] << "\n";
            return 1;
        }
    }

    phydb::PhyDB phy_db;
    phy_db.ReadLef(lef_file_name);
    phy_db.ReadDef(def_file_name);
    Circuit circuit;
    circuit.InitializeFromPhyDB(&phy_db);
    // might need to print out some circuit info here
    double hpwl_x = circuit.WeightedHPWLX();
    double hpwl_y = circuit.WeightedHPWLY();
    BOOST_LOG_TRIVIAL(info)
        << "Pin-to-Pin HPWL\n"
        << "  HPWL in the x direction: " << hpwl_x << "\n"
        << "  HPWL in the y direction: " << hpwl_y << "\n"
        << "  HPWL total:              " << hpwl_x + hpwl_y
        << "\n";
    hpwl_x = circuit.HPWLCtoCX();
    hpwl_y = circuit.HPWLCtoCY();
    BOOST_LOG_TRIVIAL(info)
        << "Center-to-Center HPWL\n"
        << "  HPWL in the x direction: " << hpwl_x << "\n"
        << "  HPWL in the y direction: " << hpwl_y << "\n"
        << "  HPWL total:              " << hpwl_x + hpwl_y
        << "\n";
    return 0;
}

void ReportUsage() {
    BOOST_LOG_TRIVIAL(info)
        << "\033[0;36m"
        << "Usage: hpwl\n"
        << " -lef <file.lef>\n"
        << " -def <file.def>\n"
        << "(order does not matter)"
        << "\033[0m\n";
}