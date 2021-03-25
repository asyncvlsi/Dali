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
  private:
    phydb::PhyDB *phy_db_ptr_ = nullptr;
    Circuit circuit_;

  public:
    Dali(phydb::PhyDB *phy_db_ptr, boost::log::trivial::severity_level sl);

    void StartPlacement(double density, int number_of_threads=1);
    void ExportToPhyDB();

    void ExportToDEF(std::string &input_def_file_full_name, std::string output_def_name = "circuit");

};

}

#endif //DALI_DALI_DALI_H_
