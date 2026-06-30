/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/

/**
 * Convert a GSRC Bookshelf placement benchmark into synthetic LEF/DEF.
 *
 * Bookshelf stores object sizes, pin offsets, row geometry, and placement in
 * separate files. It does not contain real routing-layer technology or reusable
 * cell masters. To preserve pin offsets exactly in LEF/DEF, this converter
 * emits one synthetic macro per Bookshelf object and gives that macro the pins
 * used by the object's net connections.
 */

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "dali/common/logging.h"

namespace {

struct BookshelfFiles {
  std::string nodes;
  std::string nets;
  std::string wts;
  std::string pl;
  std::string scl;
};

struct Node {
  std::string name;
  double width = 0;
  double height = 0;
  bool terminal = false;
};

struct Placement {
  double x = 0;
  double y = 0;
  std::string orient = "N";
  bool fixed = false;
};

struct NetPin {
  std::string node_name;
  std::string pin_name;
  std::string direction;
  double offset_x = 0;
  double offset_y = 0;
};

struct Net {
  std::string name;
  std::vector<NetPin> pins;
};

struct Row {
  double coordinate = 0;
  double height = 0;
  double site_width = 1;
  double site_spacing = 1;
  double subrow_origin = 0;
  int num_sites = 0;
};

struct Benchmark {
  std::vector<Node> nodes;
  std::unordered_map<std::string, size_t> node_index;
  std::unordered_map<std::string, Placement> placements;
  std::vector<Net> nets;
  std::vector<Row> rows;
  std::unordered_map<std::string, std::vector<NetPin>> pins_by_node;
  std::unordered_map<std::string, std::string> component_names;
  std::unordered_map<std::string, std::string> macro_names;
  std::unordered_map<std::string, std::string> net_names;
};

std::string StripComment(std::string line) {
  size_t pos = line.find('#');
  if (pos != std::string::npos) {
    line.resize(pos);
  }
  return line;
}

std::vector<std::string> Tokenize(std::string const& line) {
  std::istringstream stream(StripComment(line));
  std::vector<std::string> tokens;
  std::string token;
  while (stream >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

std::string DirectoryName(std::string const& path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) {
    return ".";
  }
  return path.substr(0, pos);
}

std::string BaseName(std::string const& path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) {
    return path;
  }
  return path.substr(pos + 1);
}

bool IsAbsolutePath(std::string const& path) {
  return !path.empty() && path.front() == '/';
}

std::string JoinPath(std::string const& dir, std::string const& file) {
  if (file.empty() || IsAbsolutePath(file)) {
    return file;
  }
  if (dir.empty() || dir == ".") {
    return file;
  }
  return dir + "/" + file;
}

std::string Stem(std::string path) {
  path = BaseName(path);
  size_t pos = path.find_last_of('.');
  if (pos == std::string::npos) {
    return path;
  }
  return path.substr(0, pos);
}

std::string SanitizeName(std::string const& name) {
  std::string result;
  result.reserve(name.size() + 1);
  for (char c : name) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc) || c == '_') {
      result.push_back(c);
    } else {
      result.push_back('_');
    }
  }
  if (result.empty() ||
      !(std::isalpha(static_cast<unsigned char>(result.front())) ||
        result.front() == '_')) {
    result.insert(result.begin(), '_');
  }
  return result;
}

std::string UniqueName(std::string const& name,
                       std::unordered_map<std::string, int>* used_names) {
  std::string base = SanitizeName(name);
  int& count = (*used_names)[base];
  if (count == 0) {
    ++count;
    return base;
  }
  std::string unique_name;
  do {
    unique_name = base + "_" + std::to_string(count++);
  } while (used_names->find(unique_name) != used_names->end());
  (*used_names)[unique_name] = 1;
  return unique_name;
}

double ToDouble(std::string const& value, std::string const& context) {
  try {
    return std::stod(value);
  } catch (...) {
    DaliExpects(false, "Invalid number in " << context << ": " << value);
  }
  return 0;
}

int ToInt(std::string const& value, std::string const& context) {
  try {
    return std::stoi(value);
  } catch (...) {
    DaliExpects(false, "Invalid integer in " << context << ": " << value);
  }
  return 0;
}

BookshelfFiles ParseAux(std::string const& aux_path) {
  std::ifstream input(aux_path);
  DaliExpects(input.is_open(), "Cannot open Bookshelf aux file: " << aux_path);

  BookshelfFiles files;
  std::string dir = DirectoryName(aux_path);
  std::string line;
  while (std::getline(input, line)) {
    auto tokens = Tokenize(line);
    for (auto const& token : tokens) {
      if (token.size() >= 6 && token.substr(token.size() - 6) == ".nodes") {
        files.nodes = JoinPath(dir, token);
      } else if (token.size() >= 5 &&
                 token.substr(token.size() - 5) == ".nets") {
        files.nets = JoinPath(dir, token);
      } else if (token.size() >= 4 &&
                 token.substr(token.size() - 4) == ".wts") {
        files.wts = JoinPath(dir, token);
      } else if (token.size() >= 3 && token.substr(token.size() - 3) == ".pl") {
        files.pl = JoinPath(dir, token);
      } else if (token.size() >= 4 &&
                 token.substr(token.size() - 4) == ".scl") {
        files.scl = JoinPath(dir, token);
      }
    }
  }

  DaliExpects(!files.nodes.empty(), "Aux file does not list a .nodes file");
  DaliExpects(!files.nets.empty(), "Aux file does not list a .nets file");
  DaliExpects(!files.pl.empty(), "Aux file does not list a .pl file");
  DaliExpects(!files.scl.empty(), "Aux file does not list a .scl file");
  return files;
}

void ParseNodes(std::string const& path, Benchmark* benchmark) {
  std::ifstream input(path);
  DaliExpects(input.is_open(), "Cannot open Bookshelf nodes file: " << path);

  std::string line;
  while (std::getline(input, line)) {
    auto tokens = Tokenize(line);
    if (tokens.size() < 3 || tokens[0] == "UCLA" || tokens[0] == "NumNodes" ||
        tokens[0] == "NumTerminals") {
      continue;
    }

    Node node;
    node.name = tokens[0];
    node.width = ToDouble(tokens[1], path);
    node.height = ToDouble(tokens[2], path);
    node.terminal =
        std::find(tokens.begin() + 3, tokens.end(), "terminal") != tokens.end();
    benchmark->node_index[node.name] = benchmark->nodes.size();
    benchmark->nodes.push_back(node);
  }
}

void ParsePlacements(std::string const& path, Benchmark* benchmark) {
  std::ifstream input(path);
  DaliExpects(input.is_open(),
              "Cannot open Bookshelf placement file: " << path);

  std::string line;
  while (std::getline(input, line)) {
    auto tokens = Tokenize(line);
    if (tokens.size() < 4 || tokens[0] == "UCLA") {
      continue;
    }

    Placement placement;
    placement.x = ToDouble(tokens[1], path);
    placement.y = ToDouble(tokens[2], path);
    auto colon = std::find(tokens.begin(), tokens.end(), ":");
    if (colon != tokens.end() && colon + 1 != tokens.end()) {
      placement.orient = *(colon + 1);
    }
    placement.fixed =
        std::find(tokens.begin(), tokens.end(), "/FIXED") != tokens.end();
    benchmark->placements[tokens[0]] = placement;
  }
}

void ParseNets(std::string const& path, Benchmark* benchmark) {
  std::ifstream input(path);
  DaliExpects(input.is_open(), "Cannot open Bookshelf nets file: " << path);

  std::string line;
  while (std::getline(input, line)) {
    auto tokens = Tokenize(line);
    if (tokens.empty() || tokens[0] == "UCLA" || tokens[0] == "NumNets" ||
        tokens[0] == "NumPins") {
      continue;
    }
    if (tokens[0] != "NetDegree") {
      continue;
    }

    DaliExpects(tokens.size() >= 3, "Invalid NetDegree line: " << line);
    int degree_token_index = tokens[1] == ":" ? 2 : 1;
    DaliExpects(static_cast<int>(tokens.size()) > degree_token_index,
                "Invalid NetDegree line: " << line);
    int degree = ToInt(tokens[degree_token_index], path);
    std::string net_name =
        static_cast<int>(tokens.size()) > degree_token_index + 1
            ? tokens.back()
            : "NET_" + std::to_string(benchmark->nets.size());
    Net net;
    net.name = net_name;
    net.pins.reserve(degree);

    for (int i = 0; i < degree; ++i) {
      DaliExpects(std::getline(input, line),
                  "Unexpected EOF while reading net " << net_name);
      auto pin_tokens = Tokenize(line);
      DaliExpects(pin_tokens.size() >= 5,
                  "Invalid net pin line in " << path << ": " << line);

      NetPin pin;
      pin.node_name = pin_tokens[0];
      pin.direction = pin_tokens[1];
      pin.offset_x = ToDouble(pin_tokens[3], path);
      pin.offset_y = ToDouble(pin_tokens[4], path);
      net.pins.push_back(pin);
    }

    benchmark->nets.push_back(std::move(net));
  }
}

void ParseRows(std::string const& path, Benchmark* benchmark) {
  std::ifstream input(path);
  DaliExpects(input.is_open(), "Cannot open Bookshelf scl file: " << path);

  std::string line;
  Row row;
  bool in_row = false;
  while (std::getline(input, line)) {
    auto tokens = Tokenize(line);
    if (tokens.empty()) {
      continue;
    }
    if (tokens[0] == "CoreRow") {
      row = Row{};
      in_row = true;
      continue;
    }
    if (tokens[0] == "End" && in_row) {
      benchmark->rows.push_back(row);
      in_row = false;
      continue;
    }
    if (!in_row) {
      continue;
    }

    if (tokens[0] == "Coordinate" && tokens.size() >= 3) {
      row.coordinate = ToDouble(tokens[2], path);
    } else if (tokens[0] == "Height" && tokens.size() >= 3) {
      row.height = ToDouble(tokens[2], path);
    } else if (tokens[0] == "Sitewidth" && tokens.size() >= 3) {
      row.site_width = ToDouble(tokens[2], path);
    } else if (tokens[0] == "Sitespacing" && tokens.size() >= 3) {
      row.site_spacing = ToDouble(tokens[2], path);
    } else if (tokens[0] == "SubrowOrigin" && tokens.size() >= 3) {
      row.subrow_origin = ToDouble(tokens[2], path);
      auto num_sites_it = std::find(tokens.begin(), tokens.end(), "NumSites");
      if (num_sites_it != tokens.end() && num_sites_it + 2 <= tokens.end()) {
        row.num_sites = ToInt(tokens.back(), path);
      }
    }
  }
}

void AssignLefDefNames(Benchmark* benchmark) {
  std::unordered_map<std::string, int> used_component_names;
  std::unordered_map<std::string, int> used_macro_names;
  for (auto const& node : benchmark->nodes) {
    benchmark->component_names[node.name] =
        UniqueName(node.name, &used_component_names);
    benchmark->macro_names[node.name] =
        UniqueName(node.name + "_MASTER", &used_macro_names);
  }

  std::unordered_map<std::string, int> used_net_names;
  std::unordered_map<std::string, std::unordered_map<std::string, int>>
      used_pin_names_by_node;
  for (auto& net : benchmark->nets) {
    benchmark->net_names[net.name] = UniqueName(net.name, &used_net_names);
    for (size_t pin_id = 0; pin_id < net.pins.size(); ++pin_id) {
      auto& pin = net.pins[pin_id];
      DaliExpects(
          benchmark->node_index.find(pin.node_name) !=
              benchmark->node_index.end(),
          "Net " << net.name << " references unknown node " << pin.node_name);
      auto& used_pin_names = used_pin_names_by_node[pin.node_name];
      pin.pin_name = UniqueName(net.name + "_PIN_" + std::to_string(pin_id),
                                &used_pin_names);
      benchmark->pins_by_node[pin.node_name].push_back(pin);
    }
  }
}

Benchmark LoadBenchmark(std::string const& aux_path) {
  BookshelfFiles files = ParseAux(aux_path);
  Benchmark benchmark;
  ParseNodes(files.nodes, &benchmark);
  ParsePlacements(files.pl, &benchmark);
  ParseNets(files.nets, &benchmark);
  ParseRows(files.scl, &benchmark);
  AssignLefDefNames(&benchmark);
  DaliExpects(!benchmark.nodes.empty(), "No nodes loaded from " << files.nodes);
  DaliExpects(!benchmark.rows.empty(), "No rows loaded from " << files.scl);
  return benchmark;
}

double PinX(Node const& node, NetPin const& pin) {
  return node.width / 2.0 + pin.offset_x;
}

double PinY(Node const& node, NetPin const& pin) {
  return node.height / 2.0 + pin.offset_y;
}

void WriteRect(std::ostream& out, double x, double y) {
  constexpr double kHalfPinSize = 0.5;
  out << "        RECT " << x - kHalfPinSize << " " << y - kHalfPinSize << " "
      << x + kHalfPinSize << " " << y + kHalfPinSize << " ;\n";
}

void WriteLef(Benchmark const& benchmark, std::string const& lef_path) {
  std::ofstream out(lef_path);
  DaliExpects(out.is_open(), "Cannot open output LEF file: " << lef_path);

  double site_width = benchmark.rows.front().site_width;
  double row_height = benchmark.rows.front().height;

  out << std::fixed << std::setprecision(6);
  out << "VERSION 5.8 ;\n";
  out << "BUSBITCHARS \"[]\" ;\n";
  out << "DIVIDERCHAR \"/\" ;\n";
  out << "UNITS\n  DATABASE MICRONS 1 ;\nEND UNITS\n";
  out << "MANUFACTURINGGRID 1 ;\n\n";
  out << "LAYER M1\n";
  out << "  TYPE ROUTING ;\n";
  out << "  DIRECTION HORIZONTAL ;\n";
  out << "  PITCH 1 ;\n";
  out << "  WIDTH 1 ;\n";
  out << "END M1\n\n";
  out << "SITE DALI_SITE\n";
  out << "  CLASS CORE ;\n";
  out << "  SIZE " << site_width << " BY " << row_height << " ;\n";
  out << "  SYMMETRY X Y ;\n";
  out << "END DALI_SITE\n\n";

  for (auto const& node : benchmark.nodes) {
    std::string macro_name = benchmark.macro_names.at(node.name);
    out << "MACRO " << macro_name << "\n";
    out << "  CLASS " << (node.terminal ? "BLOCK" : "CORE") << " ;\n";
    out << "  ORIGIN 0 0 ;\n";
    out << "  SIZE " << node.width << " BY " << node.height << " ;\n";
    out << "  SYMMETRY X Y ;\n";

    auto pin_it = benchmark.pins_by_node.find(node.name);
    if (pin_it != benchmark.pins_by_node.end()) {
      for (auto const& pin : pin_it->second) {
        std::string direction = pin.direction == "O" ? "OUTPUT" : "INPUT";
        out << "  PIN " << pin.pin_name << "\n";
        out << "    DIRECTION " << direction << " ;\n";
        out << "    USE SIGNAL ;\n";
        out << "    PORT\n";
        out << "      LAYER M1 ;\n";
        WriteRect(out, PinX(node, pin), PinY(node, pin));
        out << "    END\n";
        out << "  END " << pin.pin_name << "\n";
      }
    }
    out << "END " << macro_name << "\n\n";
  }
  out << "END LIBRARY\n";
}

void ComputeDieArea(Benchmark const& benchmark, double* lx, double* ly,
                    double* ux, double* uy) {
  *lx = std::numeric_limits<double>::max();
  *ly = std::numeric_limits<double>::max();
  *ux = std::numeric_limits<double>::lowest();
  *uy = std::numeric_limits<double>::lowest();
  for (auto const& row : benchmark.rows) {
    *lx = std::min(*lx, row.subrow_origin);
    *ly = std::min(*ly, row.coordinate);
    *ux = std::max(*ux, row.subrow_origin + row.num_sites * row.site_spacing);
    *uy = std::max(*uy, row.coordinate + row.height);
  }
}

void WriteDef(Benchmark const& benchmark, std::string const& def_path,
              std::string const& design_name) {
  std::ofstream out(def_path);
  DaliExpects(out.is_open(), "Cannot open output DEF file: " << def_path);

  double die_lx = 0;
  double die_ly = 0;
  double die_ux = 0;
  double die_uy = 0;
  ComputeDieArea(benchmark, &die_lx, &die_ly, &die_ux, &die_uy);

  out << std::fixed << std::setprecision(0);
  out << "VERSION 5.8 ;\n";
  out << "DIVIDERCHAR \"/\" ;\n";
  out << "BUSBITCHARS \"[]\" ;\n";
  out << "DESIGN " << SanitizeName(design_name) << " ;\n";
  out << "UNITS DISTANCE MICRONS 1 ;\n";
  out << "DIEAREA ( " << die_lx << " " << die_ly << " ) ( " << die_ux << " "
      << die_uy << " ) ;\n\n";

  int row_id = 0;
  for (auto const& row : benchmark.rows) {
    out << "ROW ROW_" << row_id++ << " DALI_SITE " << row.subrow_origin << " "
        << row.coordinate << " N DO " << row.num_sites << " BY 1 STEP "
        << row.site_spacing << " 0 ;\n";
  }

  out << "\nCOMPONENTS " << benchmark.nodes.size() << " ;\n";
  for (auto const& node : benchmark.nodes) {
    auto placement_it = benchmark.placements.find(node.name);
    Placement placement;
    if (placement_it != benchmark.placements.end()) {
      placement = placement_it->second;
    }
    bool fixed = node.terminal || placement.fixed;
    out << "  - " << benchmark.component_names.at(node.name) << " "
        << benchmark.macro_names.at(node.name) << "\n";
    out << "    + " << (fixed ? "FIXED" : "PLACED") << " ( " << placement.x
        << " " << placement.y << " ) " << SanitizeName(placement.orient)
        << " ;\n";
  }
  out << "END COMPONENTS\n\n";

  out << "NETS " << benchmark.nets.size() << " ;\n";
  for (auto const& net : benchmark.nets) {
    out << "  - " << benchmark.net_names.at(net.name) << "\n";
    for (auto const& pin : net.pins) {
      out << "    ( " << benchmark.component_names.at(pin.node_name) << " "
          << pin.pin_name << " )\n";
    }
    out << "  ;\n";
  }
  out << "END NETS\n\n";
  out << "END DESIGN\n";
}

void ReportUsage() {
  LOG(info) << "\033[0;36m"
            << "Usage: bookshelf2lefdef\n"
            << " -aux <file.aux>\n"
            << " -lef <output.lef>\n"
            << " -def <output.def>\n"
            << "(order does not matter)"
            << "\033[0m\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string aux_path;
  std::string lef_path;
  std::string def_path;

  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-aux" && i < argc) {
      aux_path = argv[i++];
    } else if (arg == "-lef" && i < argc) {
      lef_path = argv[i++];
    } else if (arg == "-def" && i < argc) {
      def_path = argv[i++];
    } else if (arg == "-h" || arg == "--help") {
      ReportUsage();
      return 0;
    } else {
      LOG(error) << "Unknown or incomplete command line option: " << arg
                 << "\n";
      ReportUsage();
      return 1;
    }
  }

  if (aux_path.empty() || lef_path.empty() || def_path.empty()) {
    ReportUsage();
    return 1;
  }

  Benchmark benchmark = LoadBenchmark(aux_path);
  std::string design_name = Stem(aux_path);
  WriteLef(benchmark, lef_path);
  WriteDef(benchmark, def_path, design_name);

  LOG(info) << "Converted Bookshelf benchmark " << aux_path << " to "
            << lef_path << " and " << def_path << "\n";
  return 0;
}
