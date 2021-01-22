//
// Created by Yihang Yang on 9/3/19.
//

#include <ctime>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <ratio>

#include "circuit.h"
#include "common/logging.h"
#include "common/si2lefdef.h"
#include "placer.h"

using namespace dali;

void PrintSoftwareStatement();
void ReportUsage();


int main(int argc, char *argv[]) {
  init_logging(boost::log::trivial::info);
  PrintSoftwareStatement();

  int num_of_thread_openmp = 1;
  omp_set_num_threads(num_of_thread_openmp);

  using std::chrono::system_clock;
  system_clock::time_point today = system_clock::now();
  std::time_t tt = system_clock::to_time_t(today);
  BOOST_LOG_TRIVIAL(info) << "today is: " << ctime(&tt) << std::endl;

  if (argc < 5) {
    ReportUsage();
    return 1;
  }
  std::string lef_file_name;
  std::string def_file_name;
  std::string cell_file_name;
  std::string output_name = "dali_out";
  std::string str_grid_value_x, str_grid_value_y;
  std::string str_target_density;
  std::string str_verbose_level;
  std::string str_x_grid;
  std::string str_y_grid;
  double x_grid = 0, y_grid = 0;
  double target_density = -1;
  bool use_naive = false;
  int io_metal_layer = 0;

  Circuit circuit;

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-n" && i < argc) {
      use_naive = true;
    } else if (arg == "-lef" && i < argc) {
      lef_file_name = std::string(argv[i++]);
      if (lef_file_name.empty()) {
        BOOST_LOG_TRIVIAL(info) << "Invalid input lef file!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-def" && i < argc) {
      def_file_name = std::string(argv[i++]);
      if (def_file_name.empty()) {
        BOOST_LOG_TRIVIAL(info) << "Invalid input def file!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-cell" && i < argc) {
      cell_file_name = std::string(argv[i++]);
      if (cell_file_name.empty()) {
        BOOST_LOG_TRIVIAL(info) << "Invalid input cell file!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-o" && i < argc) {
      output_name = std::string(argv[i++]);
      if (output_name.empty()) {
        BOOST_LOG_TRIVIAL(info) << "Invalid output name!\n";
        ReportUsage();
        return 1;
      }
    } else if ((arg == "-g" || arg == "-grid") && i < argc) {
      str_x_grid = std::string(argv[i++]);
      str_y_grid = std::string(argv[i++]);
      try {
        x_grid = std::stod(str_x_grid);
        y_grid = std::stod(str_y_grid);
        circuit.setGridValue(x_grid, y_grid);
      } catch (...) {
        BOOST_LOG_TRIVIAL(info) << "Invalid input files!\n";
        ReportUsage();
        return 1;
      }
    } else if ((arg == "-d" || arg == "-density") && i < argc) {
      str_target_density = std::string(argv[i++]);
      try {
        target_density = std::stod(str_target_density);
      } catch (...) {
        BOOST_LOG_TRIVIAL(info) << "Invalid target density!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "iolayerd" && i < argc) {
      std::string str_metal_layer_num = std::string(argv[i++]);
      try {
        io_metal_layer = std::stoi(str_metal_layer_num) - 1;
      } catch (...) {
        BOOST_LOG_TRIVIAL(info) << "Invalid metal layer number!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-v" && i < argc) {
      str_verbose_level = std::string(argv[i++]);
      int tmp;
      try {
        tmp = std::stoi(str_verbose_level);
      } catch (...) {
        tmp = -1;
      }
      if (tmp > 5 || tmp < 0) {
        BOOST_LOG_TRIVIAL(info) << "Invalid verbosity level\n";
        ReportUsage();
        return 0;
      }
      // TODO : verbose level
    } else {
      BOOST_LOG_TRIVIAL(info) << "Unknown option for file reading\n";
      BOOST_LOG_TRIVIAL(info) << arg << "\n";
      return 1;
    }
  }

  if ((lef_file_name.empty()) || (def_file_name.empty())) {
    BOOST_LOG_TRIVIAL(info) << "Invalid input files!\n";
    ReportUsage();
    return 1;
  }

  double file_wall_time = get_wall_time();
  double file_cpu_time = get_cpu_time();

  // read LEF/DEF
  readLef(lef_file_name, circuit);
  readDef(def_file_name, circuit);

  file_wall_time = get_wall_time() - file_wall_time;
  file_cpu_time = get_cpu_time() - file_cpu_time;
  BOOST_LOG_TRIVIAL(info) << "File loading complete\n";
  BOOST_LOG_TRIVIAL(info) << "(wall time: " << file_wall_time << "s, cpu time: " << file_cpu_time << "s)\n";
  circuit.ReportBriefSummary();
  circuit.ReportHPWL();

  double default_density = 0.7;
  if (target_density == -1) {
    target_density = std::max(circuit.WhiteSpaceUsage(), default_density);
  }
  if (circuit.WhiteSpaceUsage() > target_density) {
    BOOST_LOG_TRIVIAL(info) << "Cannot set target density smaller than average white space utility!\n";
    BOOST_LOG_TRIVIAL(info) << "  Average white space utility: " << circuit.WhiteSpaceUsage() << "\n";
    BOOST_LOG_TRIVIAL(info) << "  Target density: " << target_density << "\n";
    return 1;
  }
  BOOST_LOG_TRIVIAL(info) << "  Average white space utility: " << circuit.WhiteSpaceUsage() << "\n";
  BOOST_LOG_TRIVIAL(info) << "  Target density: " << target_density;
  if (target_density == default_density) {
    BOOST_LOG_TRIVIAL(info) << " (default)";
  }
  BOOST_LOG_TRIVIAL(info) << "\n";

  if (cell_file_name.empty()) {
    Placer *gb_placer = new GPSimPL;
    gb_placer->SetInputCircuit(&circuit);
    gb_placer->SetBoundaryDef();
    gb_placer->SetFillingRate(target_density);
    gb_placer->ReportBoundaries();
    gb_placer->StartPlacement();

    Placer *legalizer = new LGTetrisEx;
    legalizer->TakeOver(gb_placer);
    legalizer->StartPlacement();

    legalizer->SimpleIOPinPlacement(3);

    delete gb_placer;
    delete legalizer;
  } else {
    circuit.ReadCellFile(cell_file_name);

    Placer *gb_placer = new GPSimPL;
    gb_placer->SetInputCircuit(&circuit);
    gb_placer->SetBoundaryDef();
    gb_placer->SetFillingRate(target_density);
    gb_placer->ReportBoundaries();
    gb_placer->StartPlacement();

    Placer *legalizer = new LGTetrisEx;
    legalizer->TakeOver(gb_placer);
    legalizer->StartPlacement();

    auto *well_legalizer = new StdClusterWellLegalizer;
    well_legalizer->TakeOver(gb_placer);
    well_legalizer->StartPlacement();
    well_legalizer->GenMATLABTable("sc_result.txt");
    well_legalizer->GenMatlabClusterTable("sc_result");
    well_legalizer->GenMATLABWellTable("scw", 0);

    if (!output_name.empty()) {
      well_legalizer->EmitDEFWellFile(output_name, 1);
    }
    well_legalizer->SimpleIOPinPlacement(io_metal_layer);
    delete well_legalizer;
    delete gb_placer;
    delete legalizer;
  }
  circuit.SaveDefFile(output_name, "", def_file_name, 1, 1, 2, 1);
  circuit.SaveDefFile(output_name, "_io", def_file_name, 1, 1, 1, 1);
  circuit.SaveDefFile(output_name, "_filling", def_file_name, 1, 4, 2, 0);

  circuit.InitNetFanoutHistogram();
  circuit.ReportNetFanoutHistogram();
  circuit.ReportHPWLHistogramLinear();
  circuit.ReportHPWLHistogramLogarithm();

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;

  BOOST_LOG_TRIVIAL(info) << "****End of placement "
                          << "(wall time: " << wall_time << "s, "
                          << "cpu time: " << cpu_time << "s)****\n";

  return 0;
}

void ReportUsage() {
  BOOST_LOG_TRIVIAL(info)   << "\033[0;36m"
                            << "Usage: dali\n"
                            << "  -lef        <file.lef>\n"
                            << "  -def        <file.def>\n"
                            << "  -cell       <file.cell> (optional, if provided, iterative well placement flow will be triggered)\n"
                            << "  -o          <output_name>.def (optional, default output file name dali_out.def)\n"
                            << "  -g/-grid    grid_value_x grid_value_y (optional, default metal1 and metal2 pitch values)\n"
                            << "  -d/-density density (optional, value interval (0,1], default max(space_utility, 0.7))\n"
                            << "  -n          (optional, if this flag is present, then use naive LEF/DEF parser)\n"
                            << "  -iolayer    metal_layer_num (optional, default 1 for m1)\n"
                            << "  -v          verbosity_level (optional, 0-5, default 1)\n"
                            << "(flag order does not matter)"
                            << "\033[0m\n";
}

void PrintSoftwareStatement() {
  BOOST_LOG_TRIVIAL(info) << "\n"
                          << "+----------------------------------------------+\n"
                          << "|                                              |\n"
                          << "|     Dali: gridded cell placement flow        |\n"
                          << "|                                              |\n"
                          << "|     Department of Electrical Engineering     |\n"
                          << "|     Yale University                          |\n"
                          << "|                                              |\n"
                          << "|     Developed by                             |\n"
                          << "|     Yihang Yang, Rajit Manohar               |\n"
                          << "|                                              |\n"
                          << "|     This program is for academic use and     |\n"
                          << "|     testing only                             |\n"
                          << "|     THERE IS NO WARRANTY                     |\n"
                          << "|                                              |\n"
                          << "|     build time: " << __DATE__ << " " << __TIME__ << "         |\n"
                          << "|                                              |\n"
                          << "+----------------------------------------------+\n\n";
}
