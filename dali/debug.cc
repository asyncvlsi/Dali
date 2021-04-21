//
// Created by Yihang Yang on 5/14/19.
//

#include <ctime>
#include <unistd.h>

#include <iostream>
#include <vector>

#include "dali.h"

#define TEST_LG 0
#define TEST_PO 0
#define TEST_WELL 0
#define TEST_CLUSTER_WELL 0
#define TEST_STDCLUSTER_WELL 1

using namespace dali;

int main() {
  double wall_time = get_wall_time();

  std::string lef_file_name = "ICCAD2020/processor.lef";
  std::string def_file_name = "ICCAD2020/processor.def";
  std::string cell_file_name ="ICCAD2020/processor.cell";

  // read LEF/DEF/CELL
  phydb::PhyDB phy_db;
  phy_db.ReadLef(lef_file_name);
  phy_db.ReadDef(def_file_name);
  phy_db.ReadCell(cell_file_name);

  // initialize Dali
  Dali dali(&phy_db, boost::log::trivial::info);
  dali.StartPlacement(0.69);
  dali.SimpleIoPinPlacement("m1");
  dali.ExportToDEF(def_file_name);

  dali.ExportToPhyDB();

  wall_time = get_wall_time() - wall_time;
  BOOST_LOG_TRIVIAL(info)   << "Execution time " << wall_time << "s.\n";
  dali.Close();

  return 0;
}
