//
// Created by Yihang Yang on 3/23/21.
//

#ifndef DALI_DALI_DALI_H_
#define DALI_DALI_DALI_H_

#include <phydb/phydb.h>

#include "circuit.h"
#include "placer.h"

namespace dali {

class Dali {
  public:
    Dali(phydb::PhyDB *phy_db_ptr, std::string sl);
    Dali(phydb::PhyDB *phy_db_ptr, boost::log::trivial::severity_level sl);

    void StartPlacement(double density, int number_of_threads = 1);
    void SimpleIoPinPlacement(std::string metal_layer);
    void ExportToPhyDB();
    void Close();

    void ExportToDEF(std::string &input_def_file_full_name, std::string output_def_name = "circuit");

  private:
    phydb::PhyDB *phy_db_ptr_ = nullptr;
    Circuit circuit_;
    GPSimPL gb_placer_;
    LGTetrisEx legalizer_;
    StdClusterWellLegalizer well_legalizer_;

    void ExportComponentsToPhyDB();
    void ExportIoPinsToPhyDB();
    void ExportMiniRowsToPhyDB();
    void ExportNpPpWellToPhyDB();
};

}

#endif //DALI_DALI_DALI_H_
