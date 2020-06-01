//
// Created by Yihang Yang on 2/25/20.
//

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>

#include "circuit.h"
#include "common/global.h"
#include "common/misc.h"

VerboseLevel globalVerboseLevel = LOG_INFO;

struct TypeLayerBBox {
  BlockType *blk_type;
  double lo;
  double hi;
  double n_extra;
  double p_extra;
  double pn_edge;
  TypeLayerBBox(BlockType *type, double l, double h) : blk_type(type), lo(l), hi(h), pn_edge(-1) {}
};

void ReportUsage();

int main(int argc, char *argv[]) {
  if (argc < 7) {
    ReportUsage();
    return 1;
  }
  std::string lef_file_name;
  std::string cel_file_name;
  std::string out_file_name;
  int mode = 0;

  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-lef" && i < argc) {
      lef_file_name = std::string(argv[i++]);
    } else if (arg == "-cell" && i < argc) {
      cel_file_name = std::string(argv[i++]);
    } else if (arg == "-m" && i < argc) {
      std::string str_mode = std::string(argv[i++]);
      try {
        mode = std::stoi(str_mode);
      } catch (...) {
        mode = 0;
      }
      if (mode > 3 || mode < 0) {
        std::cout << "Invalid mode level\n";
        ReportUsage();
        return 0;
      }
    } else if (arg == "-o" && i < argc) {
      out_file_name = std::string(argv[i++]);
    } else {
      std::cout << "Unknown command line option: " << argv[i] << "\n";
      return 1;
    }
  }

  Circuit circuit;
#ifdef USE_OPENDB
  odb::dbDatabase *db = odb::dbDatabase::create();
  std::vector<std::string> defFileVec;
  odb_read_lef(db, lef_file_name.c_str());
  circuit.InitializeFromDB(db);
  circuit.ReadCellFile(cel_file_name);
#else
  circuit.ReadLefFile(lef_file_name);
#endif

  MetalLayer *hor_layer = nullptr;
  for (auto &metal_layer: circuit.tech_.metal_list) {
    if (metal_layer.Direction() == HORIZONTAL_) {
      hor_layer = &metal_layer;
      std::cout << "Horizontal metal layer is: " << *hor_layer->Name() << "\n";
      break;
    }
  }
  std::vector<TypeLayerBBox> bbox_list;
  bbox_list.reserve(circuit.tech_.block_type_map.size());
  std::ifstream ist;
  ist.open(lef_file_name.c_str());
  Assert(ist.is_open(), "Cannot open file " + lef_file_name);
  std::string line;
  std::vector<std::string> line_field;
  do {
    getline(ist, line);
    if (line.find("MACRO") != std::string::npos) {
      break;
    }
  } while (!ist.eof());

  while (!ist.eof()) {
    if (line.find("MACRO") != std::string::npos) {
      Circuit::StrSplit(line, line_field);
      std::string type_name = line_field[1];
      std::string end_macro_flag = "END " + type_name;
      BlockType *type = circuit.GetBlockType(type_name);
      double np_boundary = type->GetWell()->GetPNBoundary() * circuit.GetGridValueY();
      TypeLayerBBox type_bbox(type, np_boundary, np_boundary);
      do {
        getline(ist, line);
        if (line.find("PIN ") != std::string::npos) {
          Circuit::StrSplit(line, line_field);
          std::string pin_name = line_field[1];
          std::string end_pin_flag = "END " + pin_name;
          std::string pin_layer;
          do {
            getline(ist, line);
            if (pin_name == "Vdd" || pin_name == "GND") continue;
            if (line.find("LAYER ") != std::string::npos) {
              Circuit::StrSplit(line, line_field);
              pin_layer = line_field[1];
              //std::cout << pin_layer << " " << (pin_layer == *(hor_layer->Name())) << "\n";
            }
            if (line.find("RECT ") != std::string::npos && pin_layer == *(hor_layer->Name())) {
              Circuit::StrSplit(line, line_field);
              std::string str_ly = line_field[2];
              double ly = std::stod(str_ly);
              std::string str_uy = line_field[4];
              double uy = std::stod(str_uy);
              //std::cout << ly << " " << uy << "\n";
              if (ly < type_bbox.lo) {
                type_bbox.lo = ly;
              }
              if (uy > type_bbox.hi) {
                type_bbox.hi = uy;
              }
            }
          } while (line.find(end_pin_flag) == std::string::npos && !ist.eof());
        }
      } while (line.find(end_macro_flag) == std::string::npos && !ist.eof());
      double type_height = type->Height() * circuit.GetGridValueY();
      double pitch = hor_layer->Width() + hor_layer->Spacing();
      //std::cout << "pitch: " << pitch << "\n";
      if (mode == 0) {
        type_bbox.p_extra = std::max(pitch - type_bbox.lo, 0.0);
        type_bbox.n_extra = std::max((pitch + type_bbox.hi) - type_height, 0.0);
      } else if (mode == 1) {
        type_bbox.p_extra = 0;
        type_bbox.n_extra = 0;
      } else if (mode == 2) {
        type_bbox.p_extra = pitch;
        type_bbox.n_extra = pitch;
      } else if (mode == 3) {
        type_bbox.p_extra = 0;
        type_bbox.n_extra = pitch;
      }
      std::cout << *(type->Name()) << "  " << "size: 0 " << type_height << "\n";
      std::cout << "  " << *(hor_layer->Name()) << " y bbox: " << type_bbox.lo << " " << type_bbox.hi << "\n";
      std::cout << "  n/p extra: " << type_bbox.n_extra << " " << type_bbox.p_extra << "\n";
      bbox_list.push_back(type_bbox);
    }
    getline(ist, line);
  }
  ist.close();

  double std_n_height = 0;
  double std_p_height = 0;
  double std_height = 0;
  for (auto &type_bbox: bbox_list) {
    BlockType *type = type_bbox.blk_type;
    if (type == circuit.tech_.io_dummy_blk_type_ptr_) continue;
    double n_height = type->GetWell()->GetNWellHeight() * circuit.GetGridValueY() + type_bbox.n_extra;
    double p_height = type->GetWell()->GetPWellHeight() * circuit.GetGridValueY() + type_bbox.p_extra;
    std_n_height = std::max(std_n_height, n_height);
    std_p_height = std::max(std_p_height, p_height);
  }
  std_height = std_n_height + std_p_height;
  std::cout << "Maximum N-well height is: " << std_n_height << "\n";
  std::cout << "Maximum P-well height is: " << std_p_height << "\n";
  std::cout << "Standard height is: " << std_height << "\n";

  ist.open(lef_file_name.c_str());
  Assert(ist.is_open(), "Cannot open file " + lef_file_name);

  std::ofstream ost;
  ost.open((out_file_name + ".lef").c_str());
  Assert(ost.is_open(), "Cannot open file " + out_file_name);

  bool is_core_site_found = false;
  do {
    getline(ist, line);
    if (line.find("SITE") != std::string::npos) {
      ost << line << "\n";
      do {
        getline(ist, line);
        if (line.find("SIZE") != std::string::npos) {
          Circuit::StrSplit(line, line_field);
          ost << "    " << line_field[0] << " " << line_field[1] << " " << line_field[2] << " " << std::fixed
              << std::setprecision(6) << std_height
              << " ;\n";
        } else {
          ost << line << "\n";
        }
      } while (!ist.eof() && line.find("END") == std::string::npos);
      is_core_site_found = true;
    } else {
      ost << line << "\n";
    }
  } while (!is_core_site_found && !ist.eof());

  do {
    getline(ist, line);
    if (line.find("MACRO") != std::string::npos) {
      break;
    } else {
      ost << line << "\n";
    }
  } while (!ist.eof());

  while (!ist.eof()) {
    if (line.find("MACRO") != std::string::npos) {
      ost << line << "\n";
      Circuit::StrSplit(line, line_field);
      std::string type_name = line_field[1];
      std::string end_macro_flag = "END " + type_name;
      std::cout << "modifying " << type_name << "\n";

      TypeLayerBBox *bbox_ptr = nullptr;
      for (auto &bbox: bbox_list) {
        if (*(bbox.blk_type->Name()) == type_name) {
          bbox_ptr = &bbox;
          break;
        }
      }
      Assert(bbox_ptr != nullptr, "Cannot find type?");

      double type_p_height = circuit.GetBlockType(type_name)->GetWell()->GetPWellHeight() * circuit.GetGridValueY();
      double height_diff = std_p_height - type_p_height;
      do {
        getline(ist, line);
        if (line.find("SIZE") != std::string::npos) {
          Circuit::StrSplit(line, line_field);
          ost << "    " << line_field[0] << " " << line_field[1] << " " << line_field[2] << " " << std_height << " ;\n";
        } else if (line.find("PIN ") != std::string::npos) {
          Circuit::StrSplit(line, line_field);
          std::string pin_name = line_field[1];
          std::string end_pin_flag = "END " + pin_name;
          ost << line << "\n";
          bool hlayer_vdd_found = false;
          bool hlayer_gnd_found = false;
          do {
            getline(ist, line);
            if (pin_name == "GND") {
              std::string layer_mark = "LAYER " + *(hor_layer->Name());
              if (mode == 0 && line.find(layer_mark) != std::string::npos) {
                hlayer_gnd_found = true;
                ost << line << "\n";
                ost << "        RECT " << 0.0 << " " << 0.0 << " "
                    << bbox_ptr->blk_type->Width() * circuit.GetGridValueX() << " " << hor_layer->Width() << " ;\n";
                continue;
              }
            } else if (pin_name == "Vdd") {
              std::string layer_mark = "LAYER " + *(hor_layer->Name());
              if (mode == 0 && line.find(layer_mark) != std::string::npos) {
                hlayer_vdd_found = true;
                ost << line << "\n";
                ost << "        RECT " << 0.0 << " " << std_height - hor_layer->Width() << " "
                    << bbox_ptr->blk_type->Width() * circuit.GetGridValueX() << " " << std_height << " ;\n";
                continue;
              }
            }

            if (line.find("END") != std::string::npos && line.find(end_pin_flag) == std::string::npos ) {
              if (pin_name == "GND") {
                if (mode == 0 && !hlayer_gnd_found) {
                  ost << "        LAYER " << *hor_layer->Name() << " ;\n";
                  ost << "        RECT " << 0.0 << " " << 0.0 << " "
                      << bbox_ptr->blk_type->Width() * circuit.GetGridValueX() << " " << hor_layer->Width() << " ;\n";
                }
              } else if (pin_name == "Vdd") {
                if (mode == 0 && !hlayer_vdd_found) {
                  ost << "        LAYER " << *hor_layer->Name() << " ;\n";
                  ost << "        RECT " << 0.0 << " " << std_height - hor_layer->Width() << " "
                      << bbox_ptr->blk_type->Width() * circuit.GetGridValueX() << " " << std_height << " ;\n";
                }

              } else {}
            }

            if (line.find("RECT ") != std::string::npos) {
              Circuit::StrSplit(line, line_field);
              std::string str_ly = line_field[2];
              double ly = std::stod(str_ly);
              std::string str_uy = line_field[4];
              double uy = std::stod(str_uy);
              ly += height_diff + bbox_ptr->p_extra;
              uy += height_diff + bbox_ptr->p_extra;
              ost << "        RECT " << line_field[1] << " " << ly << " " << line_field[3] << " " << uy << " ;\n";
            } else {
              ost << line << "\n";
            }

          } while (line.find(end_pin_flag) == std::string::npos && !ist.eof());
        } else {
          ost << line << "\n";
        }
      } while (line.find(end_macro_flag) == std::string::npos && !ist.eof());

    } else {
      ost << line << "\n";
    }
    getline(ist, line);
  }

  return 0;
}

void ReportUsage() {
  std::cout << "\033[0;36m"
            << "Usage: custom2standard\n"
            << " -lef <file.lef>\n"
            << " -cell <file.cell>\n"
            << " -m mode_level (default 0)\n"
            << " -o   <file>.lef\n"
            << "(order does not matter)"
            << "\033[0m\n";
}