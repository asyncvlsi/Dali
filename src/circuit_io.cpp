//
// Created by Yihang Yang on 2019-05-19.
//

#include <cmath>

bool circuit_t::read_nodes_file(std::string const &NameOfFile) {
  size_t pos0, pos1, pos2, pos3, NumOfTerminal;
  node_t node_temp;
  std::string line, str_temp;
  NumOfTerminal = 0;
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "cannot open input file " << NameOfFile << "\n";
    return false;
  }

  AVE_WIDTH = 0;
  AVE_HEIGHT = 0;
  AVE_CELL_AREA = 0;

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
      node_temp.node_num = tmp_int;
      if (pos3 == pos2) {
        //find whether the node is a terminal or not
        node_temp.is_terminal = false;
        //if the node is not a terminal, write 0
      }
      else {
        node_temp.is_terminal = true;
        //if the node is a terminal, write 1
        NumOfTerminal += 1;
      }
      str_temp = line.substr(pos1+1, pos2-pos1-1);
      //find the width of this node
      tmp_int = stoi(str_temp);
      node_temp.w = tmp_int;
      str_temp = line.substr(pos2+1);
      //find the height of this node
      tmp_int = stoi(str_temp);
      node_temp.h = tmp_int;
      Nodelist.push_back(node_temp);
      if (node_temp.isterminal()==0) {
        AVE_WIDTH += node_temp.w;
        AVE_HEIGHT += node_temp.h;
        AVE_CELL_AREA += node_temp.w * node_temp.h;
      }
    }
  }
  ist.close();
  TERMINAL_NUM = NumOfTerminal;
  CELL_NUM = Nodelist.size() - NumOfTerminal;
  AVE_WIDTH = AVE_WIDTH/CELL_NUM;
  AVE_HEIGHT = AVE_HEIGHT/CELL_NUM;
  AVE_CELL_AREA = AVE_CELL_AREA/CELL_NUM;
  WEPSI = AVE_WIDTH/(float)100;
  HEPSI = AVE_HEIGHT/(float)100;

  /*
  std::cout << AVE_WIDTH << "\t" << AVE_HEIGHT << "\n";
  std::cout << "Node file Reading is complete\n";
  std::cout << "\t\tTotal " << Nodelist.size() << " objects (terminal " << NumOfTerminal << ")\n";
  for (size_t i=0; i<10; i++) {
    std::cout << "\to" << Nodelist[i].nodenum() << "\t" << Nodelist[i].width() << "\t" << Nodelist[i].height() << "\t" << Nodelist[i].isterminal() << "\n";
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
  for (size_t i=0; i<Nodelist.size();) {
    getline(ist, line);
    if ((line.compare(0,1,"o")==0)) {
      pos1 = line.find("\t");
      pos2 = line.find("\t", pos1+2);
      str_temp = line.substr(pos1+1, pos2-pos1-1);
      //find the low_x position of this node
      fl_temp = stof(str_temp);
      Nodelist[i].x0 = fl_temp + Nodelist[i].w/(float)2;
      str_temp = line.substr(pos2+1);
      //find the low_y position of this node
      fl_temp = stof(str_temp);
      Nodelist[i].y0 = fl_temp + Nodelist[i].h/(float)2;
      i++;
    }
  }
  ist.close();
  /*
  std::cout << "Placement file Reading is complete\n";
  std::cout << "\t\tTotal " << Nodelist.size() << " objects\n";
  for (size_t i=0; i<Nodelist.size(); i++) {
    std::cout << "\to" << Nodelist[i].nodenum() << "\t" << Nodelist[i].width() << "\t" << Nodelist[i].height() << "\t" << Nodelist[i].llx() << "\t" << Nodelist[i].lly() << "\t" << Nodelist[i].isterminal() << "\n";
  }
  */

  return true;
}

bool circuit_t::read_nets_file(std::string const &NameOfFile) {
  pininfo pin_info_temp;
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
      Netlist.push_back(net_temp);
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
      Netlist[i].nodetype.push_back(str_temp[0]);
      if (str_temp.compare(0,1,"I")==0) {
        Netlist[i].Inum += 1;
      }
      else {
        Netlist[i].Onum += 1;
      }
      pos1 = line.find(":", pos2);
      pos2 = line.find("\t", pos1);
      str_temp = line.substr(pos1+2, pos2-pos1-2);
      pin_info_temp.xoffset = stof(str_temp);
      str_temp = line.substr(pos2);
      pin_info_temp.yoffset = stof(str_temp);
      Netlist[i].pinlist.push_back(pin_info_temp);
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

bool circuit_t::read_scl_file(std::string const &NameOfFile) {
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::string line, tmp_string;
  size_t pos0, pos1;
  int num_sites = 0, num_rows = 0, last_row_top = 0, cur_row_top = 0, cur_row_height = 0;
  while (!ist.eof()) {
    getline(ist, line);
    if (line.find("NumRows")!=std::string::npos) {
      pos0 = line.find(":");
      tmp_string = line.substr(pos0+1);
      //std::cout << tmp_string << "\n";
      num_rows = std::stoi(tmp_string);
      //std::cout << "Num of rows: " << num_rows << "\n";
    }
    if ((real_BOTTOM==0) && (line.find("Coordinate")!=std::string::npos)) {
      pos0 = line.find(":");
      tmp_string = line.substr(pos0+1);
      //std::cout << tmp_string << "\n";
      real_BOTTOM = std::stoi(tmp_string);
      //std::cout << "Bottom of Core_Rows: " << real_BOTTOM << "\n";
    }
    if (line.find("Height")!=std::string::npos) {
      pos0 = line.find(":");
      tmp_string = line.substr(pos0+1);
      //std::cout << tmp_string << "\n";
      cur_row_height = std::stoi(tmp_string);
      if (std_cell_height == 0) {
        std_cell_height = cur_row_height;
        //std::cout << "Standard Cell Height: " << std_cell_height << "\n";
      } else {
        if (std_cell_height != cur_row_height) {
          //std::cout << "Error: Rows have different height\n";
          return false;
        }
      }
    }
    if ((real_LEFT==0) && (line.find("SubrowOrigin")!=std::string::npos)) {
      pos0 = line.find(":");
      pos1 = line.find("\t", pos0);
      tmp_string = line.substr(pos0+1, pos1 - pos0 -1);
      //std::cout << tmp_string << "\n";
      real_LEFT = std::stoi(tmp_string);
      //std::cout << "Left of Core_Rows: " << real_LEFT << "\n";

      pos0 = line.find(":", pos1);
      tmp_string = line.substr(pos0+1);
      //std::cout << tmp_string << "\n";
      num_sites = std::stoi(tmp_string);
      //std::cout << num_sites << "\n";
      real_RIGHT = real_LEFT + num_sites;
      //std::cout << "Right of Core_Rows: " << real_RIGHT << "\n";
    }
    if (line.find("Coordinate")!=std::string::npos) {
      pos0 = line.find(":");
      tmp_string = line.substr(pos0+1);
      //std::cout << tmp_string << "\n";
      cur_row_top = std::stoi(tmp_string);
      cur_row_top += std_cell_height;
      if (cur_row_top > last_row_top) {
        last_row_top = cur_row_top;
      }
    }
  }
  real_TOP = real_BOTTOM + std_cell_height * num_rows;
  //std::cout << "Top of Core_Rows: " << real_TOP << "\n";
  if (last_row_top != real_TOP) {
    //std::cout << "Error: Core_Row top does not match\n";
  }
  //std::cout << "Top of last Core_Row: " << last_row_top << "\n";

  /*
  for (size_t i=CELL_NUM+1; i<Nodelist.size(); i++) {
    bool out_of_region;
    out_of_region = (Nodelist[i].llx()>=real_RIGHT) || (Nodelist[i].lly()>=real_TOP) || (Nodelist[i].urx()<=real_LEFT) || (Nodelist[i].ury()<=real_BOTTOM);
    if (!out_of_region) {
      std::cout << Nodelist[i].height() % std_cell_height << " "
                << (int) (Nodelist[i].lly() - real_BOTTOM) % std_cell_height << "\n";
    }
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
  for (auto &&node: Nodelist) {
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
  for (auto &&node: Nodelist) {
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
  for (auto &&node: Nodelist) {
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
  for (auto &&node: Nodelist) {
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
  for (auto &&node: Nodelist) {
    tot_node_area += node.area();
  }

  if ((left == 0)&&(right == 0)&&(bottom == 0)&&(top == 0)) {
    // default boundary setting, a square
    int width = std::ceil(std::sqrt(tot_node_area/TARGET_FILLING_RATE));
    LEFT = (int)AVE_WIDTH;
    RIGHT = LEFT + width;
    BOTTOM = (int)AVE_WIDTH;
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