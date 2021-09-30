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
    Dali(
        phydb::PhyDB *phy_db_ptr,
        const std::string &severity_level,
        const std::string &log_file_name = "",
        const std::string &open_mode = "w"
    );
    Dali(
        phydb::PhyDB *phy_db_ptr,
        severity severity_level,
        const std::string &log_file_name = "",
        const std::string &open_mode = "w"
    );

    bool AutoIoPinPlacement();
    bool IoPinPlacement(int argc, char **argv);

    void StartPlacement(double density, int number_of_threads = 1);

    void AddWellTaps(
        phydb::Macro *cell,
        double cell_interval_microns,
        bool is_checker_board
    );
    bool AddWellTaps(int argc, char **argv);
    void GlobalPlace(double density);
    void UnifiedLegalization();

    void ExternalDetailedPlaceAndLegalize(
        std::string engine,
        bool load_dp_result = true
    );

    void ExportToPhyDB();
    void Close();

    void ExportToDEF(
        std::string &input_def_file_full_name,
        std::string output_def_name = "circuit"
    );

    IoPlacer *io_placer_ = nullptr;
    void InstantiateIoPlacer();
  private:
    Circuit circuit_;
    phydb::PhyDB *phy_db_ptr_ = nullptr;
    GPSimPL gb_placer_;
    LGTetrisEx legalizer_;
    StdClusterWellLegalizer well_legalizer_;
    WellTapPlacer *well_tap_placer_ = nullptr;

    static void ReportIoPlacementUsage();

    std::string CreateDetailedPlacementAndLegalizationScript(
        std::string &engine,
        std::string &script_name
    );

    void ExportComponentsToPhyDB();
    void ExportIoPinsToPhyDB();
    void ExportMiniRowsToPhyDB();
    void ExportPpNpToPhyDB();
    void ExportWellToPhyDB();
};

}

#endif //DALI_DALI_DALI_H_
