//
// Created by Yihang Yang on 2021-01-28.
//

#include "daliscm.h"

namespace dali {

Circuit *circuit = nullptr;

GPSimPL *gp = nullptr;
double target_density = -1;

LGTetrisEx *lg = nullptr;

StdClusterWellLegalizer *wlg = nullptr;

// currently only support single LEF/DEF file reading
bool read_lef_done = false;
bool read_def_done = false;
bool read_cell_done = false;
std::string def_file_name;

int process_runtest(int argc, char **argv) {
  if (argc > 1) {
    printf("Tama is a %s.\n", argv[1]);
  } else {
    printf("Tama is a cat.\n");
  }

  return 1;
}

int process_initDali(int argc, char **argv) {
    // initialize verbosity level
  int verbose_level = 3;
  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-v" && i < argc) {
      std::string str_verbose_level = std::string(argv[i++]);
      verbose_level = std::stoi(str_verbose_level);
      if (verbose_level > 5 || verbose_level < 0) {
        printf( "Dali expects the verbosity level 0-5\n");
        return 0;
      }
    } else {
      printf( "Unknown option for init_dali: %s\n", arg.c_str());
      return 0;
    }
  }
  boost::log::trivial::severity_level s_lvl = boost::log::trivial::info;
  switch (verbose_level) {
    case 0:s_lvl = boost::log::trivial::fatal;
      break;
    case 1:s_lvl = boost::log::trivial::error;
      break;
    case 2:s_lvl = boost::log::trivial::warning;
      break;
    case 3:s_lvl = boost::log::trivial::info;
      break;
    case 4:s_lvl = boost::log::trivial::debug;
      break;
    case 5:s_lvl = boost::log::trivial::trace;
      break;
    default:DaliExpects(false, "This is not supposed to happen");
  }
  init_logging(s_lvl);

  // instantiate a Circuit
  if (circuit != nullptr) {
    free(circuit);
  }
  circuit = new Circuit;
  read_lef_done = false;
  read_def_done = false;
  read_cell_done = false;

  // instantiate a global placer
  if (gp != nullptr) {
    free(gp);
  }
  gp = new GPSimPL;

  // instantiate a legalizer
  if (lg != nullptr) {
    free(lg);
  }
  lg = new LGTetrisEx;

  // instantiate a well legalizer
  if (wlg != nullptr) {
    free(wlg);
  }
  wlg = new StdClusterWellLegalizer;

  printf("Dali initialized\n");
  return 1;
}

int process_readLef(int argc, char **argv) {
  if (circuit == nullptr) {
    printf("Please initialize Dali before reading LEF file\n");
    return 0;
  }

  if (argc >= 2) {
    if (read_lef_done) {
      printf("Currently Dali does not support reading multiple LEF files\n");
      return 0;
    }
    std::string lef_file(argv[1]);
    readLef(lef_file, *circuit);
    read_lef_done = true;
  } else {
    printf("Usage: readLef <lefFile>\n");
  }

  return 1;
}

int process_readDef(int argc, char **argv) {
  if (circuit == nullptr) {
    printf("Please initialize Dali before reading DEF file\n");
    return 0;
  }

  if (argc >= 2) {
    if (read_def_done) {
      printf("Currently Dali does not support reading multiple DEF files\n");
      return 0;
    }
    def_file_name = std::string(argv[1]);
    readDef(def_file_name, *circuit);
    read_def_done = true;
  } else {
    printf("Usage: readDef <defFile>\n");
  }

  return 1;
}

int process_readCell(int argc, char **argv) {
  if (circuit == nullptr) {
    printf("Please initialize Dali before reading CELL file\n");
    return 0;
  }

  if (argc >= 2) {
    if (!read_lef_done) {
      printf("Please read LEF file before reading CELL file\n");
      return 0;
    }
    if (read_cell_done) {
      printf("Currently Dali does not support reading multiple CELL files\n");
      return 0;
    }
    std::string cell_file(argv[1]);
    circuit->ReadCellFile(cell_file);
    read_cell_done = true;
  } else {
    printf("Usage: readCell <cellFile>\n");
  }

  return 1;
}

int process_loadConf(int argc, char **argv) {
  if (gp == nullptr || lg == nullptr || wlg == nullptr) {
    printf("Please initialize Dali before loading placement configuration file\n");
    return 0;
  }
  if (argc >= 2) {
    std::string conf_file(argv[1]);
    gp->LoadConf(conf_file);
    lg->LoadConf(conf_file);
    wlg->LoadConf(conf_file);
  } else {
    printf("Usage: loadConf <confFile>\n");
  }

  return 1;
}

int process_setDensity(int argc, char **argv) {
  if (gp == nullptr) {
    printf("Please initialize Dali before specifying placement target density");
    return 0;
  }
  if (argc >= 2) {
    std::string str_target_density(argv[1]);
    try {
      target_density = std::stod(str_target_density);
    } catch (...) {
      printf("Invalid target density!\n");
      return 0;
    }
  } else {
    printf("Usage: setDensity <density>\n");
  }

  return 1;
}

int process_setGrid(int argc, char **argv) {
  if (circuit == nullptr) {
    printf("Please initialize Dali before setting grid values\n");
    return 0;
  }
  if (read_lef_done) {
    printf("Cannot set grid values after reading LEF file\n");
    return 0;
  }
  if (argc >= 3) {
    std::string str_x_grid = std::string(argv[1]);
    std::string str_y_grid = std::string(argv[2]);
    double x_grid = -1;
    double y_grid = -1;
    try {
      x_grid = std::stod(str_x_grid);
      y_grid = std::stod(str_y_grid);
    } catch (...) {
      printf("Invalid grid values!\n");
      return 0;
    }
    circuit->setGridValue(x_grid, y_grid);
  } else {
    printf("Usage: setGrid <grid_value_x, grid_value_y>\n");
  }

  return 1;
}


int process_globalPlace(int argc, char **argv) {
  gp->SetInputCircuit(circuit);
  gp->SetBoundaryDef();
  gp->SetFillingRate(target_density);
  gp->ReportBoundaries();
  gp->StartPlacement();
  return 1;
}

int process_legalize(int argc, char **argv) {
  lg->TakeOver(gp);
  lg->StartPlacement();
  return 1;
}

int process_wellLegalize(int argc, char **argv) {
  wlg->TakeOver(gp);
  wlg->StartPlacement();
  return 1;
}

int process_savePlace(int argc, char **argv) {
  if (circuit == nullptr) {
    printf("Please initialize Dali before saving DEF file\n");
    return 0;
  }
  if (argc >= 2) {
    std::string output_name(argv[1]);
    circuit->SaveDefFile(output_name, "", def_file_name, 1, 1, 2, 1);
    circuit->SaveDefFile(output_name, "_io", def_file_name, 1, 1, 1, 1);
    circuit->SaveDefFile(output_name, "_filling", def_file_name, 1, 4, 2, 0);
  } else {
    printf("Usage: saveDef <outputDefFile>\n");
  }
  return 1;
}

int process_closeDali(int argc, char **argv) {
  close_logging();

  free(circuit);
  circuit = nullptr;

  free(gp);
  gp = nullptr;
  target_density = -1;

  free(lg);
  lg = nullptr;

  free(wlg);
  wlg = nullptr;

  read_lef_done = false;
  read_def_done = false;
  read_cell_done = false;

  printf("Dali closed\n");
  return 1;
}

}