//
// Created by Yihang Yang on 2019-03-26.
//

#include <cmath>
#include <fstream>
#include <iostream>
#include "circuit.hpp"

circuit_t::circuit_t() {
  tot_movable_num = 0;
  tot_unmovable_num = 0;
  HPWL = 0;
}

bool circuit_t::add_to_block_list(block_t &block) {
  if (block_name_map.find(block.name()) != block_name_map.end()) {
    block_name_map.insert(std::pair<std::string, int>(block.name(), block.num()));
    block_list.push_back(block);
    return true;
  } else {
    std::cout << "Existing block in block_list with name: " << block.name() << "\n";
    return false;
  }
}

bool circuit_t::add_to_net_list(net_t &net) {
  net_list.push_back(net);
  return true;
}

block_t* circuit_t::create_empty_entry_block_list() {
  block_t new_block;
  block_list.push_back(new_block);
  size_t block_list_size = block_list.size();
  return &block_list[block_list_size-1];
}

net_t* circuit_t::create_empty_entry_net_list() {
  net_t new_net;
  net_list.push_back(new_net);
  size_t net_list_size = net_list.size();
  return &net_list[net_list_size-1];
}

bool circuit_t::read_nodes_file(std::string const &NameOfFile) {
  size_t pos0, pos1, pos2, pos3, NumOfTerminal;
  block_t node_temp;
  std::string line, str_temp;
  NumOfTerminal = 0;
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "cannot open input file " << NameOfFile << "\n";
    return false;
  }

  ave_width = 0;
  ave_height = 0;
  ave_cell_area = 0;

  int tmp_int;
  while (!ist.eof()) {
    getline(ist, line);
    if ((line.compare(0,1,"\t")==0)) {
      pos0 = line.find("\t");
      pos1 = line.find("\t",pos0+1);
      pos2 = line.find("\t",pos1+1);
      pos3 = line.rfind("\t");
      str_temp = line.substr(pos0+2, pos1-pos0-2);//find the number of the node
      tmp_int = stoi(str_temp);
      node_temp.set_block_num(tmp_int);
      if (pos3 == pos2) {
        //find whether the node is a terminal or not
        node_temp.set_movable(false);
        //if the node is not a terminal, write 0
      }
      else {
        node_temp.set_movable(true);
        //if the node is a terminal, write 1
        NumOfTerminal += 1;
      }
      str_temp = line.substr(pos1+1, pos2-pos1-1);
      //find the width of this node
      tmp_int = stoi(str_temp);
      node_temp.set_width(tmp_int);
      str_temp = line.substr(pos2+1);
      //find the height of this node
      tmp_int = stoi(str_temp);
      node_temp.set_height(tmp_int);
      block_list.push_back(node_temp);
      if (node_temp.is_movable()) {
        ave_width += node_temp.width();
        ave_height += node_temp.height();
        ave_cell_area += node_temp.width() * node_temp.height();
      }
    }
  }
  ist.close();
  tot_unmovable_num = NumOfTerminal;
  tot_movable_num = block_list.size() - NumOfTerminal;
  ave_width = ave_width/tot_movable_num;
  ave_height = ave_height/tot_movable_num;
  ave_cell_area = ave_cell_area/tot_movable_num;
  /*
  std::cout << ave_width << "\t" << ave_height << "\n";
  std::cout << "Node file Reading is complete\n";
  std::cout << "\t\tTotal " << block_list.size() << " objects (terminal " << NumOfTerminal << ")\n";
  for (size_t i=0; i<10; i++) {
    std::cout << "\to" << block_list[i].nodenum() << "\t" << block_list[i].width() << "\t" << block_list[i].height() << "\t" << block_list[i].isterminal() << "\n";
  }*/

  return true;
}

bool circuit_t::read_pl_file(std::string const &NameOfFile) {
  size_t pos1, pos2;
  float fl_temp;
  std::string line, str_temp;
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "cannot open input file " << NameOfFile << "\n";
    return false;
  }
  for (size_t i=0; i<block_list.size();) {
    getline(ist, line);
    if ((line.compare(0,1,"o")==0)) {
      pos1 = line.find("\t");
      pos2 = line.find("\t", pos1+2);
      str_temp = line.substr(pos1+1, pos2-pos1-1);
      //find the low_x position of this node
      fl_temp = stof(str_temp);
      block_list[i].set_llx(fl_temp);
      str_temp = line.substr(pos2+1);
      //find the low_y position of this node
      fl_temp = stof(str_temp);
      block_list[i].set_lly(fl_temp);
      i++;
    }
  }
  ist.close();
  /*
  std::cout << "Placement file Reading is complete\n";
  std::cout << "\t\tTotal " << block_list.size() << " objects\n";
  for (size_t i=0; i<block_list.size(); i++) {
    std::cout << "\to" << block_list[i].nodenum() << "\t" << block_list[i].width() << "\t" << block_list[i].height() << "\t" << block_list[i].llx() << "\t" << block_list[i].lly() << "\t" << block_list[i].isterminal() << "\n";
  }
  */

  return true;
}

bool circuit_t::read_nets_file(std::string const &NameOfFile) {
  pin_t pin_info_temp;
  size_t pos1, pos2, pos3, NumPins=0;
  int int_temp=0;
  std::string line, str_temp;
  net_t net_temp;
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "cannot open input file " << NameOfFile << "\n";
    return false;
  }
  size_t i=0;
  while (!ist.eof()) {
    getline(ist, line);
    if (line.compare(0,3,"Net")==0) {
      pos1 = line.find(":");
      pos2 = line.find(" ", pos1+2);
      pos3 = line.find("n", pos2+1);
      str_temp = line.substr(pos3+1);
      //find the number of this net net_num
      int_temp = stoi(str_temp);
      net_temp.net_num = (size_t)int_temp;
      i = net_temp.net_num;
      str_temp = line.substr(pos1+1, pos2-pos1-1);
      //find the number of pins in this net p
      int_temp = stoi(str_temp);
      net_temp.p = (size_t)int_temp;
      NumPins += net_temp.p;
      if (net_temp.p >= 2) {
        net_temp.invpmin1 = 1/(float)(net_temp.p-1);
      }
      else {
        net_temp.invpmin1 = 0;
      }
      net_list.push_back(net_temp);
    }
    else if (line.compare(0,1,"\t")==0) {
      // find all the pins in this net, including Node number and xoffset, y offset,
      // the offset is with respect to the center of each node
      pos1 = line.find("\t");
      pos2 = line.find("\t", pos1+1);
      str_temp = line.substr(pos1+2, pos2-pos1-2);
      int_temp = stoi(str_temp);
      pin_info_temp.pinnum = (size_t)int_temp;
      str_temp = line.substr(pos2+1, 1);
      net_list[i].nodetype.push_back(str_temp[0]);
      if (str_temp.compare(0,1,"I")==0) {
        net_list[i].Inum += 1;
      }
      else {
        net_list[i].Onum += 1;
      }
      pos1 = line.find(":", pos2);
      pos2 = line.find("\t", pos1);
      str_temp = line.substr(pos1+2, pos2-pos1-2);
      pin_info_temp.xoffset = stof(str_temp);
      str_temp = line.substr(pos2);
      pin_info_temp.yoffset = stof(str_temp);
      net_list[i].pinlist.push_back(pin_info_temp);
    }
  }
  ist.close();

  /*
  std::cout << "net_t file processing is done\n";
  std::cout << "Total " << Netslist.size() << " nets " << NumPins << " pins\n";
  std::ofstream ost("adaptec_reconstructed.nets");
  for (size_t j=0; j<Netslist.size(); j++) {
    ost << "NetDegree : " << Netslist[j].p << "\tn" << Netslist[j].net_num << "\tInum:" << Netslist[j].Inum << "\tOnum:" << Netslist[j].Onum << "\n";
    for (size_t k=0; k<Netslist[j].pinlist.size(); k++) {
      ost << "\to" << Netslist[j].pinlist[k].pinnum << "\t" << Netslist[j].nodetype[k] << " : " << std::setiosflags(std::ios::fixed) << std::setprecision(6) << Netslist[j].pinlist[k].xoffset << "\t" << Netslist[j].pinlist[k].yoffset << "\n";
    }
    if (Netslist[j].Onum==0) std::cout << j << "\n";
  }
  */

  return true;
}

bool circuit_t::write_pl_solution(std::string const &NameOfFile) {
  std::ofstream ost(NameOfFile.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  for (auto &&node: block_list) {
    if (node.isterminal()==0) {
      ost << "o" << node.nodenum() << "\t" << node.llx() << "\t" << node.lly() << "\t:\tN\n";
    }
    else {
      ost << "o" << node.nodenum() << "\t" << node.llx() << "\t" << node.lly() << "\t:\tN\t/FIXED\n";
    }
  }
  ost.close();
  //std::cout << "Output solution file complete\n";
  return true;
}

bool circuit_t::write_pl_anchor_solution(std::string const &NameOfFile) {
  std::ofstream ost(NameOfFile.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  for (auto &&node: block_list) {
    if (node.isterminal()==0) {
      ost << "o" << node.nodenum() << "\t" << node.anchorx - node.w/2.0 << "\t" << node.anchory - node.h/2.0 << "\t:\tN\n";
    }
    else {
      ost << "o" << node.nodenum() << "\t" << node.anchorx - node.w/2.0 << "\t" << node.anchory - node.h/2.0 << "\t:\tN\t/FIXED\n";
    }
  }
  ost.close();
  //std::cout << "Output solution file complete\n";
  return true;
}

bool circuit_t::write_node_terminal(std::string const &NameOfFile, std::string const &NameOfFile1) {
  std::ofstream ost(NameOfFile.c_str());
  std::ofstream ost1(NameOfFile1.c_str());
  if ((ost.is_open()==0)||(ost1.is_open()==0)) {
    std::cout << "Cannot open file" << NameOfFile << " or " << NameOfFile1 <<  "\n";
    return false;
  }
  for (auto &&node: block_list) {
    if (node.isterminal()==0) {
      ost1 << node.x0 << "\t" << node.y0 << "\n";
    }
    else {
      double low_x, low_y, width, height;
      width = node.width();
      height = node.height();
      low_x = node.llx();
      low_y = node.lly();
      for (int j=0; j<height; j++) {
        ost << low_x << "\t" << low_y+j << "\n";
        ost << low_x+width << "\t" << low_y+j << "\n";
      }
      for (int j=0; j<width; j++) {
        ost << low_x+j << "\t" << low_y << "\n";
        ost << low_x+j << "\t" << low_y+height << "\n";
      }
    }
  }
  ost.close();
  ost1.close();
  return true;
}

bool circuit_t::write_anchor_terminal(std::string const &NameOfFile, std::string const &NameOfFile1) {
  std::ofstream ost(NameOfFile.c_str());
  std::ofstream ost1(NameOfFile1.c_str());
  if ((ost.is_open()==0)||(ost1.is_open()==0)) {
    std::cout << "Cannot open file" << NameOfFile << " or " << NameOfFile1 <<  "\n";
    return false;
  }
  for (auto &&node: block_list) {
    if (node.isterminal()==0) {
      ost1 << node.anchorx << "\t" << node.anchory << "\n";
    }
    else {
      double low_x, low_y, width, height;
      width = node.width();
      height = node.height();
      low_x = node.llx();
      low_y = node.lly();
      for (int j=0; j<height; j++) {
        ost << low_x << "\t" << low_y+j << "\n";
        ost << low_x+width << "\t" << low_y+j << "\n";
      }
      for (int j=0; j<width; j++) {
        ost << low_x+j << "\t" << low_y << "\n";
        ost << low_x+j << "\t" << low_y+height << "\n";
      }
    }
  }
  ost.close();
  ost1.close();
  return true;
}

bool circuit_t::set_filling_rate(float rate) {
  if (rate >=1 ) {
    return false;
  } else {
    TARGET_FILLING_RATE = rate;
  }
  return true;
}

bool circuit_t::set_boundary(int left, int right, int bottom, int top) {
  int tot_node_area = 0;
  int area;
  for (auto &&node: block_list) {
    tot_node_area += node.area();
  }

  if ((left == 0)&&(right == 0)&&(bottom == 0)&&(top == 0)) {
    // default boundary setting, a square
    int width = std::ceil(std::sqrt(tot_node_area/TARGET_FILLING_RATE));
    LEFT = (int)ave_width;
    RIGHT = LEFT + width;
    BOTTOM = (int)ave_width;
    TOP = BOTTOM + width;
    area = width * width;
    std::cout << "Pre-set filling rate: " << TARGET_FILLING_RATE << "\n";
    TARGET_FILLING_RATE = tot_node_area/(float)area;
    std::cout << "Adjusted filling rate: " << TARGET_FILLING_RATE << "\n";
  } else {
    area = (right - left) * (top - bottom);
    // check if area is large enough and update
    if (area <= tot_node_area) {
      std::cout << "Error: defined boundaries have smaller area than total cell area\n";
      return false;
    } else {
      TARGET_FILLING_RATE = tot_node_area/(float)area;
    }
  }

  return true;
}
