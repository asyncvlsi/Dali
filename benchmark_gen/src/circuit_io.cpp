//
// Created by Yihang Yang on 2019-05-16.
//

#include <fstream>
#include <iomanip>      // std::setprecision

bool circuit_t::write_node_file(std::string const &NameOfFile) {
  std::ofstream ost;
  ost.open(NameOfFile);
  if (!ost.is_open()) {
    std::cout << "Error: Cannot open node file " << NameOfFile << "!!!\n";
    return false;
  }
  ost << "NumNodes : \t\t" << Netlist.size() << "\n";
  ost << "NumTerminals : \t\t" << 0 << "\n";
  for (auto &&node: Nodelist) {
    ost << "\to" << node.node_num << "\t" << node.w << "\t" << node.h << "\n";
  }

  return true;
}

bool circuit_t::write_net_file(std::string const &NameOfFile) {
  std::ofstream ost;
  ost.open(NameOfFile);
  if (!ost.is_open()) {
    std::cout << "Error: Cannot open node file " << NameOfFile << "!!!\n";
    return false;
  }
  size_t total_pin_num = 0;
  for (auto &&net: Netlist) {
    total_pin_num += net.pinlist.size();
  }
  ost << "NumNets : " << Netlist.size() << "\n";
  ost << "NumPins : " << total_pin_num << "\n\n";
  for (auto &&net: Netlist) {
    ost << "NetDegree : "<< net.pinlist.size() << "   n" << net.net_num << "\n";
    ost << std::fixed;
    for (auto &&pin: net.pinlist) {
      ost << "\to" << pin.pinnum << "\tO : " << std::setprecision(6) << pin.xoffset - Nodelist[pin.pinnum].w/2.0
          << "\t" << std::setprecision(6) << pin.yoffset - Nodelist[pin.pinnum].h/2.0 << "\n";
    }
  }

  return true;
}

bool circuit_t::write_pl_file(std::string const &NameOfFile) {
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
