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

struct FlowStep {
  explicit FlowStep(char *cmd_name): cmd(cmd_name) {}
  char *cmd;
  std::vector<FlowStep *> next;
};
FlowStep *cur_step = nullptr;
std::vector<FlowStep *> all_steps;
std::unordered_map<char *, FlowStep *> step_map;

void AddStep(char *step_name) {
  if (step_map.find(step_name)!=step_map.end()) {
    BOOST_LOG_TRIVIAL(fatal) << "Adding Dali steps multiple times: " << step_name << std::endl;
    return;
  }
  auto *step = new FlowStep(step_name);
  all_steps.push_back(step);
  step_map[step_name] = step;
}

void AddTransition(char *head_step_name, char *tail_step_name) {
  if (step_map.find(head_step_name)==step_map.end()) return;
  if (step_map.find(tail_step_name)==step_map.end()) return;
  step_map[head_step_name]->next.push_back(step_map[tail_step_name]);
}

void ConstructDaliFlow() {
  if (!all_steps.empty()) return;

  all_steps.reserve(20);
  AddStep(initDali_str);
  AddStep(readLEF_str);
  AddStep(readDEF_str);
  AddStep(readCell_str);
  AddStep(loadConf_str);
  AddStep(setDensity_str);
  AddStep(globalPlace_str);
  AddStep(legalize_str);
  AddStep(wellLegalize_str);
  AddStep(savePlace_str);
  AddStep(closeDali_str);

  AddTransition(initDali_str, readLEF_str);
  AddTransition(readLEF_str, readDEF_str);
  AddTransition(readDEF_str, readCell_str);
  AddTransition(readCell_str, loadConf_str);
  AddTransition(readCell_str, setDensity_str);
  AddTransition(loadConf_str, setDensity_str);
  AddTransition(setDensity_str, globalPlace_str);
  AddTransition(globalPlace_str, legalize_str);
  AddTransition(legalize_str, wellLegalize_str);
  AddTransition(wellLegalize_str, savePlace_str);
  AddTransition(savePlace_str, closeDali_str);
  AddTransition(closeDali_str, initDali_str);

  for (auto &step_ptr: all_steps) {
    std::cout << "Step name: " << step_ptr->cmd << "\n";
    std::cout << "    Legal next step: ";
    for (auto &next_step_ptr: step_ptr->next) {
      std::cout << next_step_ptr->cmd << "  ";
    }
    std::cout << "\n";
  }
}

bool CheckStepLegal(const char *step_to_execute) {
  if (cur_step == nullptr) {
    if (step_to_execute != initDali_str) {
      printf("Next legal step: %s", initDali_str);
      return false;
    }
    return true;
  } else {
    for (auto &next_step_ptr: cur_step->next) {
      if (next_step_ptr->cmd == step_to_execute) return true;
    }
    printf("Next legal steps: \n");
    for (auto &next_step_ptr: cur_step->next) {
      printf("    %s\n", next_step_ptr->cmd);
    }
  }
  return false;
}

int process_runtest(int argc, char **argv) {
  if (argc > 1) {
    printf("Tama is a %s.\n", argv[1]);
  } else {
    printf("Tama is a cat.\n");
  }

  return 1;
}

int process_initDali(int argc, char **argv) {
  ConstructDaliFlow();

  if (!CheckStepLegal(initDali_str)) return 0;
  cur_step = step_map[initDali_str];
  // initialize verbosity level
  int verbose_level = 3;
  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-v" && i < argc) {
      std::string str_verbose_level = std::string(argv[i++]);
      verbose_level = std::stoi(str_verbose_level);
      if (verbose_level > 5 || verbose_level < 0) {
        printf("Dali expects the verbosity level 0-5\n");
        return 0;
      }
    } else {
      printf("Unknown option for init_dali: %s\n", arg.c_str());
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
  if (!CheckStepLegal(readLEF_str)) return 0;
  cur_step = step_map[readLEF_str];
  if (circuit == nullptr) {
    printf("Please initialize Dali before reading LEF file\n");
    return 0;
  }

  if (argc==5 && strcmp(argv[3], "-g")==0) {
    std::string str_x_grid = std::string(argv[3]);
    std::string str_y_grid = std::string(argv[4]);
    double x_grid = -1;
    double y_grid = -1;
    try {
      x_grid = std::stod(str_x_grid);
      y_grid = std::stod(str_y_grid);
    } catch (...) {
      printf("Invalid grid values!\n");
      return 0;
    }
    circuit->SetGridValue(x_grid, y_grid);
  }

  if (argc >= 2) {
    if (read_lef_done) {
      printf("Currently Dali does not support reading multiple LEF files\n");
      return 0;
    }
    std::string lef_file(argv[1]);

    ReadLEF(lef_file, circuit);
    read_lef_done = true;
  } else {
    printf("Usage: readLEF <lefFile> [-g grid_value_x, grid_value_y]\n");
  }

  return 1;
}

int process_readDef(int argc, char **argv) {
  if (!CheckStepLegal(readDEF_str)) return 0;
  cur_step = step_map[readDEF_str];

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
    ReadDEF(def_file_name, circuit);
    read_def_done = true;
  } else {
    printf("Usage: readDEF <defFile>\n");
  }

  return 1;
}

int process_readCell(int argc, char **argv) {
  if (!CheckStepLegal(readCell_str)) return 0;
  cur_step = step_map[readCell_str];

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
  if (!CheckStepLegal(loadConf_str)) return 0;
  cur_step = step_map[loadConf_str];

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
  if (!CheckStepLegal(setDensity_str)) return 0;
  cur_step = step_map[setDensity_str];

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

int process_globalPlace(int argc, char **argv) {
  if (!CheckStepLegal(globalPlace_str)) return 0;
  cur_step = step_map[globalPlace_str];

  gp->SetInputCircuit(circuit);
  gp->SetBoundaryDef();
  gp->SetFillingRate(target_density);
  gp->ReportBoundaries();
  gp->StartPlacement();
  return 1;
}

int process_legalize(int argc, char **argv) {
  if (!CheckStepLegal(legalize_str)) return 0;
  cur_step = step_map[legalize_str];

  lg->TakeOver(gp);
  lg->StartPlacement();
  return 1;
}

int process_wellLegalize(int argc, char **argv) {
  if (!CheckStepLegal(wellLegalize_str)) return 0;
  cur_step = step_map[wellLegalize_str];

  wlg->TakeOver(gp);
  wlg->StartPlacement();
  return 1;
}

int process_savePlace(int argc, char **argv) {
  if (!CheckStepLegal(savePlace_str)) return 0;
  cur_step = step_map[savePlace_str];

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
  if (!CheckStepLegal(closeDali_str)) return 0;
  cur_step = nullptr;

  close_logging();

  if (circuit!=nullptr) free(circuit);
  circuit = nullptr;

  if (gp!= nullptr) free(gp);
  gp = nullptr;
  target_density = -1;

  if (lg!= nullptr) free(lg);
  lg = nullptr;

  if (wlg!= nullptr) free(wlg);
  wlg = nullptr;

  read_lef_done = false;
  read_def_done = false;
  read_cell_done = false;

  step_map.clear();
  for (auto &step_ptr: all_steps) {
    free(step_ptr);
  }
  all_steps.clear();

  printf("Dali closed\n");
  return 1;
}

}