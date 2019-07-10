//
// Created by Yihang Yang on 2019-03-26.
//

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include "circuit.h"
#include "debug.h"

circuit_t::circuit_t() {
  _tot_movable_num = 0;
  _ave_width = 0;
  _ave_height = 0;
  _tot_block_area = 0;
}

/*
 * ... array somewhere with some size ...
 * block_t **allblocks;
 * int num_blocks = 0;
 * int max_blocks = 0;
 *
 * if (max_blocks == num_blocks) {
 *      if (max_blocks == 0) {
 *          allblocks = (block_t **) malloc (sizeof (block_t *)*1024);
 *          // check for NULL result, error
 *          max_blocks = 1024;
 *      }
 *      else {
 *            max_blocks = max_blocks + 1024;
 *           allblocks = (block_t **) realloc (sizeof (block_t *)*max_blocks);
 *           // check for null result
 *      }
 *  }
 *  allblocks[num_blocks] = new block_t (w, h, llx, lly, movable);
 *  num_blocks++;
 *  allblocks[num_blocks-1]->set_num (num_blocks-1);
 *  return allblocks[num_blocks-1];
 *
 */

void circuit_t::set_dummy_space(int init_ds_x, int init_ds_y) {
  _dummy_space_x = init_ds_x;
  _dummy_space_y = init_ds_y;
}

int circuit_t::dummy_space_x() {
  return _dummy_space_x;
}

int circuit_t::dummy_space_y() {
  return _dummy_space_y;
}


bool circuit_t::add_new_block(std::string &blockName, int w, int h, int llx, int lly, bool movable, std::string typeName) {
  if (block_name_map.find(blockName) == block_name_map.end()) {
    size_t block_list_size = block_list.size();
    block_t tmp_block(blockName, w, h, llx, lly, movable, std::move(typeName));
    tmp_block.set_num(block_list_size);
    block_name_map.insert(std::pair<std::string, int>(tmp_block.name(), tmp_block.num()));
    block_list.push_back(tmp_block);
    return true;
  } else {
    #ifdef USEDEBUG
    std::cout << "Error!\n";
    std::cout << "Existing block in block_list with name: " << blockName << "\n";
    #endif
    return false;
  }
}

bool circuit_t::create_blank_net(std::string &netName, double weight) {
  if (net_name_map.find(netName) == net_name_map.end()) {
    net_t tmp_net(netName, weight);
    size_t net_list_size = net_list.size();
    tmp_net.set_num(net_list_size);
    net_name_map.insert(std::pair<std::string, int>(tmp_net.name(), tmp_net.num()));
    net_list.push_back(tmp_net);
    return true;
  } else {
    #ifdef USEDEBUG
    std::cout << "Error!\n";
    std::cout << "Existing net in net_list with name: " << netName << "\n";
    #endif
    return false;
  }
}

bool circuit_t::add_pin_to_net(const std::string &netName, const std::string &blockName, int xOffset, int yOffset, std::string pinName) {
  if (net_name_map.find(netName) == net_name_map.end()) {
    #ifdef USEDEBUG
    std::cout << "Error!\n";
    std::cout << "No net in net_list has name: " << netName << "\n";
    #endif
    return false;
  }
  if (block_name_map.find(blockName) == block_name_map.end()){
    #ifdef USEDEBUG
    std::cout << "Error!\n";
    std::cout << "No block in block_list has name: " << blockName << "\n";
    #endif
    return false;
  }

  int block_num = block_name_map.find(blockName)->second;
  pin_t tmp_pin(xOffset, yOffset, &block_list[block_num], std::move(pinName));

  int net_num = net_name_map.find(netName)->second;
  return net_list[net_num].add_pin(tmp_pin);

}


void circuit_t::parse_line(std::string &line, std::vector<std::string> &field_list) {
  std::vector<char> delimiter_list;
  delimiter_list.push_back(' ');
  delimiter_list.push_back(':');
  delimiter_list.push_back(';');
  delimiter_list.push_back('\t');
  delimiter_list.push_back('\r');
  delimiter_list.push_back('\n');

  field_list.clear();
  std::string empty_str;
  bool is_delimiter, old_is_delimiter = true;
  int current_field = -1;
  for (auto &&c: line) {
    is_delimiter = false;
    for (auto &&delimiter: delimiter_list) {
      if (c == delimiter) {
        is_delimiter = true;
        break;
      }
    }
    if (is_delimiter) {
      old_is_delimiter = is_delimiter;
      continue;
    } else {
      if (old_is_delimiter) {
        current_field++;
        field_list.push_back(empty_str);
      }
      field_list[current_field] += c;
      old_is_delimiter = is_delimiter;
    }
  }
}

bool circuit_t::read_nodes_file(std::string const &NameOfFile) {
  int tmp_w, tmp_h; // width and height
  bool tmp_movable = true;
  std::string tmp_name; // name
  std::string line, tmp_string;
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "Cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::cout << "Start reading node file\n";

  while (!ist.eof()) {
    std::vector<std::string> block_field;
    getline(ist, line);
    if (line.find("#")!=std::string::npos) {
      continue;
    }
    parse_line(line, block_field);
    if (block_field.size()<=2) {
      std::cout << "\t#" << line << "\n";
      continue;
    }
    tmp_name = block_field[0];
    try {
      tmp_w = std::stoi(block_field[1]);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion:" << block_field[1] << "\n";
      std::cout << line << "\n";
      return false;
    }
    try {
      tmp_h = std::stoi(block_field[2]);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion: " << block_field[2] << "\n";
      std::cout << line << "\n";
      return false;
    }

    tmp_movable = true;
    if (block_field.size() >= 4) {
      if (block_field[3].find("terminal")!=std::string::npos) {
        tmp_movable = false;
      }
    }

    if (!add_new_block(tmp_name, tmp_w, tmp_h, 0, 0, tmp_movable)) {
      return false;
    }
  }
  ist.close();

  return true;
}

void circuit_t::report_block_list() {
  for (auto &&block: block_list) {
    std::cout << block << "\n";
  }
}

void circuit_t::report_block_map() {
  for (auto &&name_num_pair: block_name_map) {
    std::cout << name_num_pair.first << " " << name_num_pair.second << "\n";
  }
}

bool circuit_t::read_nets_file(std::string const &NameOfFile) {
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "Cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::cout << "Start reading net file\n";
  std::string line;
  std::string tmp_net_name;

  /***here is a converter, converting ISPD file format from pin offset with respect to the center of a cell to lower left corner***/
  /*** start ***/
  //std::string new_file_name = NameOfFile + "_after_conversion";
  //std::ofstream ost(new_file_name.c_str());
  /*** To be continued ***/

  while (true) {
    getline(ist, line);
    if (line.find("NetDegree") != std::string::npos) {
      goto I_DONT_LIKE_THIS_BUT;
    } else {
      std::cout << "\t#" << line << "\n";
    }
  }
  while (!ist.eof()) {
    getline(ist, line);
    if (line.empty()) {
      continue;
    }
    I_DONT_LIKE_THIS_BUT:if (line.find("NetDegree") != std::string::npos) {
      std::vector<std::string> net_head_field;
      parse_line(line, net_head_field);
      //for (auto &&field: net_head_field) std::cout << field << "---";
      //std::cout << "\n";
      if (net_head_field.size() < 3) {
        std::cout << "Error!\n";
        std::cout << "Invalid input, filed number less than 3, expecting at least 3\n";
        std::cout << "\t" << line << "\n";
        return false;
      }
      tmp_net_name = net_head_field[2];
      create_blank_net(tmp_net_name);

      /*****cont'*****/
      //ost << line << "\n";
      /*****to be continues*****/

    } else {
      std::string tmp_block_name;
      int tmp_x_offset, tmp_y_offset;
      //double tmp_x_offset, tmp_y_offset;
      std::vector<std::string> pin_field;
      parse_line(line, pin_field);
      //for (auto &&field: pin_field) std::cout << field << "---";
      //std::cout << "\n";
      if (pin_field.size() < 4) {
        std::cout << "Error!\n";
        std::cout << "Invalid input, filed number less than 4, expecting at least 4\n";
        std::cout << "\t" << line << "\n";
        return false;
      }

      tmp_block_name = pin_field[0];
      try {
        tmp_x_offset = std::stoi(pin_field[2]);
        //tmp_x_offset = std::stod(pin_field[2]);
      } catch (...) {
        std::cout << "Error!\n";
        std::cout << "Invalid stoi conversion:" << pin_field[2] << "\n";
        std::cout << "\t" << line << "\n";
        return false;
      }
      try {
        tmp_y_offset = std::stoi(pin_field[3]);
        //tmp_y_offset = std::stod(pin_field[3]);
      } catch (...) {
        std::cout << "Error!\n";
        std::cout << "Invalid stoi conversion: " << pin_field[3] << "\n";
        std::cout << "\t" << line << "\n";
        return false;
      }
      if (!add_pin_to_net(tmp_net_name, tmp_block_name, tmp_x_offset, tmp_y_offset)) {
        return false;
      }

      /*****cont'*****/
      //size_t block_num = block_name_map.find(tmp_block_name)->second;
      //ost << "    " << pin_field[0] << "\t" << pin_field[1] << " : " << tmp_x_offset + block_list[block_num].width()/2.0 << "\t" << tmp_y_offset + block_list[block_num].height()/2.0 << "\n";
      /*****to be continues*****/

    }
  }
  ist.close();

  /*****cont'*****/
  //ost.close();

  return true;
}

void circuit_t::report_net_list() {
  for (auto &&net: net_list) {
    std::cout << net << "\n";
  }
}

void circuit_t::report_net_map() {
  for (auto &&name_num_pair: net_name_map) {
    std::cout << name_num_pair.first << " " << name_num_pair.second << "\n";
  }
}

bool circuit_t::read_pl_file(std::string const &NameOfFile) {
  block_t *block;
  int tmp_block_index;
  int tmp_x, tmp_y;
  std::string line, tmp_name;
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "Cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::cout << "Start reading placement file\n";

  while (!ist.eof()) {
    getline(ist, line);
    if (line.find("#")!=std::string::npos) {
      continue;
    }
    std::vector<std::string> block_field;
    parse_line(line, block_field);
    if (block_field.size() < 4) {
      std::cout << "\t#" << line << "\n";
      continue;
    }
    tmp_name = block_field[0];
    if (block_name_map.find(tmp_name) == block_name_map.end()) {
      std::cout << "Warning: Cannot find block with name: " << tmp_name << "\n";
      std::cout << "Ignoring line:\n";
      std::cout << "\t" << line << "\n";
      continue;
    }
    tmp_block_index = block_name_map.find(tmp_name)->second;
    block = &block_list[tmp_block_index];
    try {
      tmp_x = std::stoi(block_field[1]);
      block->set_llx(tmp_x);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion:" << block_field[1] << "\n";
      std::cout << line << "\n";
      return false;
    }
    try {
      tmp_y = std::stoi(block_field[2]);
      block->set_lly(tmp_y);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion: " << block_field[2] << "\n";
      std::cout << line << "\n";
      return false;
    }
    block->set_orientation(block_field[3]);
    if (line.find("FIXED") != std::string::npos) {
      block->set_movable(false);
    }
  }
  ist.close();
  return true;
}

bool circuit_t::add_block_type(std::string &blockTypeName, int width, int height) {
  if (blockTypeNameMap.find(blockTypeName) == blockTypeNameMap.end()) {
    size_t blockTypeMapSize = blockTypeNameMap.size();
    blockTypeNameMap.insert(std::pair<std::string, int>(blockTypeName, blockTypeMapSize));
    blockTypeList.resize(blockTypeMapSize + 1);
    blockTypeList.back().set_name(blockTypeName);
    blockTypeList.back().set_width(width);
    blockTypeList.back().set_height(height);
    blockTypeList.back().set_num(blockTypeMapSize);
    return true;
  } else {
    #ifdef USEDEBUG
    std::cout << "Error!\n";
    std::cout << "Existing block type in blockTypeList with name: " << blockTypeName << "\n";
    #endif
    return false;
  }
}

bool circuit_t::add_pin_to_block(std::string &blockTypeName, std::string &pinName, int xOffset, int yOffset) {
  if (blockTypeNameMap.find(blockTypeName) == blockTypeNameMap.end()) {
    #ifdef USEDEBUG
    std::cout << "Error!\n";
    std::cout << "Adding pin to block\n";
    std::cout << "No block type exists in blockTypeList has name: " << blockTypeName << "\n";
    #endif
    return false;
  }
  int blockTypeNum = blockTypeNameMap.find(blockTypeName)->second;
  return blockTypeList[blockTypeNum].add_pin(pinName, xOffset, yOffset);
}

bool circuit_t::add_new_block(std::string &blockName, std::string &blockTypeName, int llx, int lly, bool movable) {
  if (blockTypeNameMap.find(blockTypeName) == blockTypeNameMap.end()) {
    #ifdef USEDEBUG
    std::cout << "Error!\n";
    std::cout << "No block type exists in blockTypeList has name: " << blockTypeName << "\n";
    #endif
    return false;
  }
  if (block_name_map.find(blockName) != block_name_map.end()) {
    #ifdef USEDEBUG
    std::cout << "Error!\n";
    std::cout << "Block in blockList already: " << blockName << "\n";
    #endif
    return false;
  }
  int blockTypeNum = blockTypeNameMap.find(blockTypeName)->second;
  int width = blockTypeList[blockTypeNum].width();
  int height = blockTypeList[blockTypeNum].height();
  return add_new_block(blockName, width, height, llx, lly, movable, blockTypeName);
}

bool circuit_t::add_pin_to_net(std::string &netName, std::string &blockName, std::string &pinName) {
  if (net_name_map.find(netName) == net_name_map.end()) {
    #ifdef USEDEBUG
    std::cout << "Error!\n";
    std::cout << "No net in net_list has name: " << netName << "\n";
    #endif
    return false;
  }
  if (block_name_map.find(blockName) == block_name_map.end()) {
    #ifdef USEDEBUG
    std::cout << "Error!\n";
    std::cout << "No block in blockList has name: " << blockName << "\n";
    #endif
    return false;
  }
  block_type_t *tmp_type = &blockTypeList[blockTypeNameMap.find(block_list[block_name_map.find(blockName)->second].type())->second];
  if (tmp_type->pinname_num_map.find(pinName) == tmp_type->pinname_num_map.end()) {
    #ifdef USEDEBUG
    std::cout << "Error!\n";
    std::cout << "No pin: " << pinName << " in block: " << blockName << " with type: " << tmp_type->name() << "\n";
    #endif
    return false;
  }
  int tmp_xOffset = tmp_type->pin_list[tmp_type->pinname_num_map.find(pinName)->second].x;
  int tmp_yOffset = tmp_type->pin_list[tmp_type->pinname_num_map.find(pinName)->second].y;
  return add_pin_to_net(netName, blockName, tmp_xOffset+dummy_space_x()/2, tmp_yOffset+dummy_space_y()/2, pinName);
}

bool circuit_t::read_lef_file(std::string const &NameOfFile) {
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "Cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::cout << "Start reading lef file\n";

  std::string line;
  lef_database_microns = 0;
  while ((lef_database_microns == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DATABASE MICRONS")!=std::string::npos) {
      std::vector<std::string> line_field;
      parse_line(line, line_field);
      if (line_field.size() < 3) {
        std::cout << "Error!\n";
        std::cout << "Invalid UNITS declaration: expecting 3 fields\n";
        return false;
      }
      try {
        lef_database_microns = std::stoi(line_field[2]);
      } catch (...) {
        std::cout << "Error!\n";
        std::cout << "Invalid stoi conversion:" << line_field[2] << "\n";
        std::cout << line << "\n";
        return false;
      }
    }
  }
  //std::cout << "DATABASE MICRONS " << lef_database_microns << "\n";

  m2_pitch = 0;
  while ((m2_pitch == 0) && !ist.eof()) {
    getline(ist, line);
    if(line.find("LAYER Metal2")!=std::string::npos) {
      //std::cout << line << "\n";
      do {
        getline(ist, line);
        //std::cout << line << "\n";
        if (line.find("PITCH") != std::string::npos) {
          std::vector<std::string> line_field;
          parse_line(line, line_field);
          if (line_field.size() < 3) {
            std::cout << "Error!\n";
            std::cout << "Invalid LAYER Metal2 PITCH declaration: expecting 3 fields\n";
            return false;
          }
          try {
            m2_pitch = std::stod(line_field[2]);
          } catch (...) {
            std::cout << "Error!\n";
            std::cout << "Invalid stoi conversion:" << line_field[2] << "\n";
            std::cout << line << "\n";
            return false;
          }
        }
      } while (line.find("END Metal2")==std::string::npos && !ist.eof());
      break;
    }
  }

  while (!ist.eof()) {
    getline(ist, line);
    if (line.find("MACRO") != std::string::npos) {
      std::vector<std::string> line_field;
      parse_line(line, line_field);
      if (line_field.size() < 2) {
        std::cout << "Error!\n";
        std::cout << "Invalid type name: expecting 2 fields\n";
        std::cout << line << "\n";
        return false;
      }
      std::string blockTypeName = line_field[1];
      //std::cout << blockTypeName << "\n";
      int width = 0, height = 0;
      std::string end_macro_flag = "END " + line_field[1];
      do {
        getline(ist, line);
        while ((width==0) && (height==0) && !ist.eof()) {
          if (line.find("SIZE") != std::string::npos) {
            std::vector<std::string> size_field;
            parse_line(line, size_field);
            try {
              width = std::stoi(size_field[1]);
            } catch (...) {
              std::cout << "Error!\n";
              std::cout << "Invalid stod conversion:" << size_field[1] << "\n";
              std::cout << line << "\n";
              return false;
            }
            try {
              height = std::stoi(size_field[3]);
            } catch (...) {
              std::cout << "Error!\n";
              std::cout << "Invalid stod conversion:" << size_field[3] << "\n";
              std::cout << line << "\n";
              return false;
            }
            width = (int)(std::round(width/m2_pitch));
            height = (int)(std::round(height/m2_pitch));
            add_block_type(blockTypeName, width, height);
            //std::cout << "  type width, height: "
            //          << blockTypeList.back().width() << " "
            //          << blockTypeList.back().height() << "\n";
          }
          getline(ist, line);
        }

        if (line.find("PIN") != std::string::npos) {
          std::vector<std::string> pin_field;
          parse_line(line, pin_field);
          if (pin_field.size() < 2) {
            std::cout << "Error!\n";
            std::cout << "Invalid pin name: expecting 2 fields\n";
            std::cout << line << "\n";
            return false;
          }
          std::string pinName = pin_field[1];
          std::string end_pin_flag = "END " + pinName;

          // skip to "PORT" rectangle list
          do {
            getline(ist, line);
          }  while (line.find("PORT")==std::string::npos && !ist.eof());

          int tmp_xoffset = -1, tmp_yoffset = -1;
          do {
            getline(ist, line);
            if (line.find("RECT") != std::string::npos) {
              //std::cout << line << "\n";
              if ((tmp_xoffset == -1) && (tmp_yoffset == -1)) {
                std::vector<std::string> rect_field;
                parse_line(line, rect_field);
                if (rect_field.size() < 5) {
                  std::cout << "Error!\n";
                  std::cout << "Invalid rect definition: expecting 5 fields\n";
                  std::cout << line << "\n";
                  return false;
                }
                tmp_xoffset = (int)(std::round(std::stod(rect_field[1])/m2_pitch));
                tmp_yoffset = (int)(std::round(std::stod(rect_field[4])/m2_pitch));
                //std::cout << "  " << tmp_xoffset << " " << tmp_yoffset << "\n";
                add_pin_to_block(blockTypeName, pinName, tmp_xoffset, tmp_yoffset);
              } else {
                continue;
              }
            }
          } while (line.find(end_pin_flag)==std::string::npos && !ist.eof());
        }

      } while (line.find(end_macro_flag)==std::string::npos && !ist.eof());
      if (blockTypeList.back().pin_list.empty()) {
        std::cout << "Error!\n";
        std::cout << "MACRO: " << blockTypeList.back().name() << " has no pin!\n";
        return false;
      }
    }
  }

  std::cout << "lef file reading complete\n";
  return true;
}

void circuit_t::report_blockType_list() {
  for (auto &&block_type: blockTypeList) {
    std::cout << block_type << "\n";
  }
}

void circuit_t::report_blockType_map() {
  for (auto &&blockTypeMap: blockTypeNameMap) {
    std::cout << blockTypeMap.first << " " << blockTypeMap.second << "\n";
  }
}

bool circuit_t::read_def_file(std::string const &NameOfFile) {
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "Cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::cout << "Start reading def file\n";

  std::string line;
  def_distance_microns = 0;
  while ((def_distance_microns == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DISTANCE MICRONS")!=std::string::npos) {
      std::vector<std::string> line_field;
      parse_line(line, line_field);
      if (line_field.size() < 4) {
        std::cout << "Error!\n";
        std::cout << "Invalid UNITS declaration: expecting 4 fields\n";
        return false;
      }
      try {
        def_distance_microns = std::stoi(line_field[3]);
      } catch (...) {
        std::cout << "Error!\n";
        std::cout << "Invalid stoi conversion:" << line_field[3] << "\n";
        std::cout << line << "\n";
        return false;
      }
    }
  }
  //std::cout << "DISTANCE MICRONS " << def_distance_microns << "\n";

  def_left = 0;
  def_right = 0;
  def_bottom = 0;
  def_top = 0;
  while ((def_left == 0) && (def_right == 0) && (def_bottom == 0) && (def_top == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DIEAREA")!=std::string::npos) {
      std::vector<std::string> die_area_field;
      parse_line(line, die_area_field);
      //std::cout << line << "\n";
      if (die_area_field.size() < 9) {
        std::cout << "Error!\n";
        std::cout << "Invalid UNITS declaration: expecting 9 fields\n";
        return false;
      }
      try {
        def_left = (int)std::round(std::stoi(die_area_field[2])/m2_pitch/def_distance_microns);
        def_bottom = (int)std::round(std::stoi(die_area_field[3])/m2_pitch/def_distance_microns);
        def_right = (int)std::round(std::stoi(die_area_field[6])/m2_pitch/def_distance_microns);
        def_top = (int)std::round(std::stoi(die_area_field[7])/m2_pitch/def_distance_microns);
      } catch (...) {
        std::cout << "Error!\n";
        std::cout << "Invalid stoi conversion: expecting DIEAREA ( XXX XXX ) ( XXX XXX ) ;\n";
        std::cout << line << "\n";
        return false;
      }
    }
  }
  //std::cout << "DIEAREA ( " << def_left << " " << def_bottom << " ) ( " << def_right << " " << def_top << " )\n";

  while ((line.find("COMPONENTS") == std::string::npos) && !ist.eof()) {
    getline(ist, line);
  }
  //std::cout << line << "\n";
  getline(ist, line);
  while ((line.find("END COMPONENTS") == std::string::npos) && !ist.eof()) {
    //std::cout << line << "\t";
    std::vector<std::string> block_declare_field;
    parse_line(line, block_declare_field);
    if (block_declare_field.empty()) {
      continue;
    } else if (block_declare_field.size() < 3) {
      std::cout << "Error!\n";
      std::cout << "Invalid block declaration, expecting at least: - compName modelName\n";
      return false;
    }
    //std::cout << block_declare_field[0] << " " << block_declare_field[1] << "\n";
    if (blockTypeNameMap.find(block_declare_field[2]) == blockTypeNameMap.end()) {
      std::cout << "Error!\n";
      std::cout << "Invalid block declaration, no such type exists\n";
      std::cout << line << "\n";
      return false;
    } else {
      block_type_t *blockType = &blockTypeList[blockTypeNameMap.find(block_declare_field[2])->second];
      std::string blockName = blockType->name();
      add_new_block(block_declare_field[1], blockName, 0, 0, true);
    }
    getline(ist, line);
  }

  while (line.find("NETS") == std::string::npos && !ist.eof()) {
    getline(ist, line);
  }
  //std::cout << line << "\n";
  getline(ist, line);
  // the following is a hack now, cannot handle all cases, probably need to use BISON in the future if necessary
  while ((line.find("END NETS") == std::string::npos) && !ist.eof()) {
    if (line.find("-") != std::string::npos) {
      //std::cout << line << "\n";
      std::vector<std::string> net_field;
      parse_line(line, net_field);
      if (net_field.size() < 2) {
        std::cout << "Error!\n";
        std::cout << "Invalid net declaration, expecting at least: - netName\n";
        return false;
      }
      //std::cout << "\t" << net_field[0] << " " << net_field[1] << "\n";
      if (net_field[1].find("Reset") != std::string::npos) {
        //std::cout << net_field[1] << "\n";
        create_blank_net(net_field[1], _global_signal_weight);
      } else {
        create_blank_net(net_field[1], _normal_signal_weight);
      }
      getline(ist, line);
      //std::cout << line << "\n";
      std::vector<std::string> pin_field;
      parse_line(line, pin_field);
      if ((pin_field.size()%4 != 0) || (pin_field.size() < 8)) {
        std::cout << "Error!\n";
        std::cout << "Invalid net declaration, expecting 4n fields, where n >= 2\n";
        return false;
      }
      for (size_t i=0; i<pin_field.size(); i += 4) {
        //std::cout << "     " << pin_field[i+1] << " " << pin_field[i+2];
        add_pin_to_net(net_field[1], pin_field[i+1], pin_field[i+2]);
      }
      //std::cout << "\n";
    }
    getline(ist, line);
  }
  std::cout << "def file reading complete\n";
  return true;
}

double circuit_t::ave_width_real_time() {
  _ave_width = 0;
  for (auto &&block: block_list) {
    _ave_width += block.width();
  }
  _ave_width /= block_list.size();
  return _ave_width;
}

double circuit_t::ave_height_real_time() {
  _ave_height = 0;
  for (auto &&block: block_list) {
    _ave_height += block.height();
  }
  _ave_height /= block_list.size();
  return _ave_height;
}

int circuit_t::tot_block_area_real_time() {
  _tot_block_area = 0;
  for (auto &&block: block_list) {
    _tot_block_area += block.area();
  }
  return _tot_block_area;
}

double circuit_t::ave_block_area_real_time() {
  return tot_block_area_real_time()/(double)block_list.size();
}

int circuit_t::tot_movable_num_real_time() {
  _tot_movable_num = 0;
  for (auto &&block: block_list) {
    if (block.is_movable()) {
      _tot_movable_num++;
    }
  }
  return _tot_movable_num;
}

int circuit_t::tot_unmovable_num_real_time() {
  return block_list.size() - tot_movable_num_real_time();
}

int circuit_t::reportHPWL() {
  int HPWL = 0;
  for (auto &&net: net_list) {
    HPWL += net.hpwl();
  }
  return HPWL;
}

double circuit_t::ave_width() {
  if (_ave_width < 1e-5) {
    return ave_width_real_time();
  }
  return _ave_width;
}

double circuit_t::ave_height() {
  if (_ave_height < 1e-5) {
    return ave_height_real_time();
  }
  return _ave_height;
}

int circuit_t::tot_block_area() {
  if (_tot_block_area == 0) {
    return tot_block_area_real_time();
  }
  return _tot_block_area;
}

double circuit_t::ave_block_area() {
  return tot_block_area()/(double)block_list.size();
}

int circuit_t::tot_movable_num() {
  if (_tot_movable_num == 0) {
    return tot_movable_num_real_time();
  }
  return _tot_movable_num;
}

int circuit_t::tot_unmovable_num() {
  return block_list.size() - _tot_movable_num;
}

bool circuit_t::write_nodes_file(std::string const &NameOfFile) {
  std::ofstream ost(NameOfFile.c_str());
  if (!ost.is_open()) {
    std::cout << "Cannot open file " << NameOfFile << "\n";
    return false;
  }

  for (auto &&block: block_list) {
    ost << "\t" << block.name() << "\t" << block.width() << "\t" << block.height() << "\n";
  }
  ost.close();

  return true;
}

bool circuit_t::write_nets_file(std::string const &NameOfFile) {
  std::ofstream ost(NameOfFile.c_str());
  if (!ost.is_open()) {
    std::cout << "Cannot open file " << NameOfFile << "\n";
    return false;
  }

  for (auto &&net: net_list) {
    ost << "NetDegree : " << net.p() << "\t" << net.name() << "\n";
    for (auto &&pin: net.pin_list) {
      ost << "\t" << pin.name() << "\tI : " << pin.x_offset() << "\t" << pin.y_offset() << "\n";
    }
  }
  ost.close();

  return true;
}

bool circuit_t::save_DEF(std::string const &NameOfFile, std::string const &defFileName) {
  std::ofstream ost(NameOfFile.c_str());
  if (!ost.is_open()) {
    std::cout << "Cannot open file " << NameOfFile << "\n";
    return false;
  }

  std::ifstream ist(defFileName.c_str());
  if (!ist.is_open()) {
    std::cout << "Cannot open file " << defFileName << "\n";
    return false;
  }

  std::string line;
  // 1. print file header, copy from def file
  while (line.find("COMPONENTS") == std::string::npos && !ist.eof()) {
    getline(ist,line);
    ost << line << "\n";
  }

  // 2. print component
  //std::cout << _circuit->block_list.size() << "\n";
  for (auto &&block: block_list) {
    ost << "- "
        << block.name() << " "
        << block.type() << " + "
        << block.place_status() << " "
        << "( " + std::to_string((int)(block.llx()*def_distance_microns*m2_pitch)) + " " + std::to_string((int)(block.lly()*def_distance_microns*m2_pitch)) + " )" << " "
        << block.orientation() + " ;\n";
  }
  ost << "END COMPONENTS\n";
  // jump to the end of components
  while (line.find("END COMPONENTS") == std::string::npos && !ist.eof()) {
    getline(ist,line);
  }

  // 3. print net, copy from def file
  while (!ist.eof()) {
    getline(ist,line);
    ost << line << "\n";
  }
  /*
  ost << "NETS " << net_list.size() << " ;\n";
  for (auto &&net: net_list) {
    ost << "- "
        << net.name() << "\n";
    ost << " ";
    for (auto &&pin: net.pin_list) {
      ost << " " << pin.pin_name();
    }
    ost << "\n" << " ;\n";
  }
  ost << "END NETS\n\n";

  ost << "END DESIGN\n";
   */

  ost.close();
  ist.close();
  return true;
}

bool circuit_t::gen_matlab_disp_file(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&block: block_list) {
    ost << "rectangle('Position',[" << block.llx() << " " << block.lly() << " " << block.width() - dummy_space_x() << " " << block.height() - dummy_space_y() << "], 'LineWidth', 1, 'EdgeColor','blue')\n";
  }
  for (auto &&net: net_list) {
    for (size_t i=0; i<net.pin_list.size(); i++) {
      for (size_t j=i+1; j<net.pin_list.size(); j++) {
        ost << "line([" << net.pin_list[i].abs_x() << "," << net.pin_list[j].abs_x() - dummy_space_x()/2 << "],[" << net.pin_list[i].abs_y() - dummy_space_y()/2 << "," << net.pin_list[j].abs_y() << "],'lineWidth', 0.5)\n";
      }
    }
  }
  ost << "rectangle('Position',[" << def_left << " " << def_bottom << " " << def_right - def_left << " " << def_top - def_bottom << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}
