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


#include "../include/Eigen/Sparse"

extern size_t CELLNUM; // number of movable cells
extern size_t TERMINALNUM;

typedef Eigen::SparseMatrix<double> SpMat; // declares a column-major sparse matrix type of double
typedef Eigen::Triplet<double> T; // A triplet is a simple object representing a non-zero entry as the triplet: row index, column index, value.

void global_placer(std::vector<Node> &Nodeslist, std::vector<Net> &Netslist);
void buildProblemx(const std::vector<Node> &Nodeslist, const std::vector<Net> &Netslist, std::vector<T>& coefficients, Eigen::VectorXd& b);
void buildProblemy(const std::vector<Node> &Nodeslist, const std::vector<Net> &Netslist, std::vector<T>& coefficients, Eigen::VectorXd& b);
void saveinNodelist(std::vector<Node> &Nodeslist, const Eigen::VectorXd& x, const Eigen::VectorXd& y);


void global_placer(std::vector<Node> &Nodeslist, std::vector<Net> &Netslist) {
  std::cout << CELLNUM << "\t" << TERMINALNUM << "\n";
  // Assembly:
  std::vector<T> coefficientsx, coefficientsy;            // list of non-zeros coefficients
  Eigen::VectorXd x(CELLNUM), bx(CELLNUM), y(CELLNUM), by(CELLNUM);             // the solution and the right hand side-vector resulting from the constraints
  SpMat Ax(CELLNUM,CELLNUM), Ay(CELLNUM,CELLNUM);				// sparse matrix
  // fill A and b
  buildProblemx(Nodeslist, Netslist, coefficientsx, bx);
  buildProblemy(Nodeslist, Netslist, coefficientsy, by);
  Ax.setFromTriplets(coefficientsx.begin(), coefficientsx.end());
  Ay.setFromTriplets(coefficientsy.begin(), coefficientsy.end());
  // Solving:
  Eigen::ConjugateGradient<SpMat, Eigen::Lower|Eigen::Upper> cg;
  cg.compute(Ax);
  x = cg.solve(bx);
  //std::cout << "Here is the vector x:\n" << x << std::endl;
  std::cout << "#iterations:     " << cg.iterations() << std::endl;
  std::cout << "estimated error: " << cg.error()      << std::endl;
  cg.compute(Ay);
  y = cg.solve(by);
  std::cout << "#iterations:     " << cg.iterations() << std::endl;
  std::cout << "estimated error: " << cg.error()      << std::endl;
  saveinNodelist(Nodeslist, x, y);
  return;
}

void buildProblemx(const std::vector<Node> &Nodeslist, const std::vector<Net> &Netslist, std::vector<T>& coefficients, Eigen::VectorXd& b) {
  size_t tempnodenum1, tempnodenum2;
  for (size_t i=0; i<Netslist.size(); i++) {
    if (Netslist[i].p==1) continue;
    for (size_t j=0; j<Netslist[i].nodelist.size(); j++) {
      tempnodenum1 = Netslist[i].nodelist[j];
      if (Nodeslist[tempnodenum1].isterminal()==1) break;
      for (size_t k=j+1; k<Netslist[i].nodelist.size(); k++) {
        tempnodenum2 = Netslist[i].nodelist[k];
        if (Nodeslist[tempnodenum2].isterminal()==0) {
          coefficients.push_back(T(tempnodenum1,tempnodenum1,1));
          coefficients.push_back(T(tempnodenum2,tempnodenum2,1));
          coefficients.push_back(T(tempnodenum1,tempnodenum2,-1));
          coefficients.push_back(T(tempnodenum2,tempnodenum1,-1));
        }
        else {
          coefficients.push_back(T(tempnodenum1,tempnodenum1,1));
          b(tempnodenum1) += Nodeslist[tempnodenum2].llx() + Nodeslist[tempnodenum2].width()/2.0;
        }
      }
    }
  }
  return;
}

void buildProblemy(const std::vector<Node> &Nodeslist, const std::vector<Net> &Netslist, std::vector<T>& coefficients, Eigen::VectorXd& b) {
  size_t tempnodenum1, tempnodenum2;
  for (size_t i=0; i<Netslist.size(); i++) {
    if (Netslist[i].p==1) continue;
    for (size_t j=0; j<Netslist[i].nodelist.size(); j++) {
      tempnodenum1 = Netslist[i].nodelist[j];
      if (Nodeslist[tempnodenum1].isterminal()==1) break;
      for (size_t k=j+1; k<Netslist[i].nodelist.size(); k++) {
        tempnodenum2 = Netslist[i].nodelist[k];
        if (Nodeslist[tempnodenum2].isterminal()==0) {
          coefficients.push_back(T(tempnodenum1,tempnodenum1,1));
          coefficients.push_back(T(tempnodenum2,tempnodenum2,1));
          coefficients.push_back(T(tempnodenum1,tempnodenum2,-1));
          coefficients.push_back(T(tempnodenum2,tempnodenum1,-1));
        }
        else {
          coefficients.push_back(T(tempnodenum1,tempnodenum1,1));
          b(tempnodenum1) += Nodeslist[tempnodenum2].lly() + Nodeslist[tempnodenum2].height()/2.0;
        }
      }
    }
  }
  return;
}