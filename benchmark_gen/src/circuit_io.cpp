//
// Created by Yihang Yang on 2019-05-16.
//

#include <fstream>

bool circuit_t::write_node_file(std::string const &NameOfFile) {
  std::ofstream ost;
  ost.open(NameOfFile);
  if (!ost.is_open()) {
    std::cout << "Error: Cannot open node file " << NameOfFile << "!!!\n";
    return false;
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
