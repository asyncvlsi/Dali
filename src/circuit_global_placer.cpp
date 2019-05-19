#include <cmath>
#include <random>
#include "circuit_node_net.hpp"

void circuit_t::uniform_initialization() {
  /*
  size_t last_terminal_index = Nodelist.size()-1;
  int left = (int)Nodelist[last_terminal_index].llx();
  int right = (int)Nodelist[last_terminal_index].urx();
  int bottom = (int)Nodelist[last_terminal_index].lly();
  int top = (int)Nodelist[last_terminal_index].ury();
  // search boundaries
  for (auto &&node: Nodelist) {
    if (node.isterminal()==0) continue;
    if (node.llx() < left)   left = (int)node.llx();
    if (node.urx() > right)  right = (int)node.urx();
    if (node.lly() < bottom) bottom = (int)node.lly();
    if (node.ury() > top)    top = (int)node.ury();
  }
  int Nx = right-left;
  int Ny = top-bottom;
  LEFT = left;
  RIGHT = right;
  BOTTOM = bottom;
  TOP = top;
   */

  LEFT = real_LEFT;
  RIGHT = real_RIGHT;
  BOTTOM = real_BOTTOM;
  TOP = real_TOP;

  int Nx = RIGHT - LEFT;
  int Ny = TOP - BOTTOM;

  std::default_random_engine generator{0};
  std::uniform_real_distribution<float> distribution(0, 1);
  for (auto &&node: Nodelist) {
    if (node.isterminal()) break;
    node.x0 = LEFT + Nx*distribution(generator);
    // uniform distribution around the center of placement region
    node.y0 = BOTTOM + Ny*distribution(generator);
  }
  /*
  std::cout << "Initializing node locations is complete\n";
  std::cout << "\t\tTotal " << Nodelist.size() << " objects\n";
  for (size_t i=0; i<Nodelist.size(); i++) {
      std::cout << "\to" << Nodelist[i].nodenum() << "\t" << Nodelist[i].width() << "\t" << Nodelist[i].height() << "\t" << Nodelist[i].x0 << "\t" << Nodelist[i].y0 << "\t" << Nodelist[i].x1 << "\t" << Nodelist[i].y1 << "\t" << Nodelist[i].isterminal() << "\n";
  }
  */
}

void circuit_t::initial_placement() {
  uniform_initialization();
  /* give each node a initial location, which is random inside the placement region, defined by LEFT, RIGHT, BOTTOM, TOP */
  update_max_min_node_x();
  update_max_min_node_y();
  /* update HPWLX, HPWLY, prepare for problem building */
  HPWLx_converge = false;
  HPWLy_converge = false;
  /* set HPWLx_converge and HPWLy_converge to false */
  for (int i=0; ; i++) {
    if (HPWLx_converge && HPWLy_converge) break;
    if (!HPWLx_converge) {
      //build_problem_clique_x();
      //build_problem_b2b_x_nooffset();
      build_problem_b2b_x();
      // fill elements into matrix Ax, bx
      CG_solver_x();
      // solve the linear equation for x direction
      update_max_min_node_x();
    }

    if (!HPWLy_converge) {
      //build_problem_clique_y();
      //build_problem_b2b_y_nooffset();
      build_problem_b2b_y();
      // fill elements into matrix Ay, by
      CG_solver_y();
      // solve the linear equation for y direction
      update_max_min_node_y();
    }
    if (HPWLx_converge && HPWLy_converge) break;
  }
  /*for (size_t i=0; i<CELL_NUM; i++) {
    if ((Nodelist[i].llx() < LEFT) || (Nodelist[i].urx() > RIGHT) || (Nodelist[i].lly() < BOTTOM) || (Nodelist[i].ury() > TOP)) {
      std::cout << "Final outboundary" << i << "\n";
    }
  }*/
  std::cout << "Initial Placement Complete\n";
  std::cout << "HPWL: " << HPWLX_new + HPWLY_new << "\n";
}

void circuit_t::look_ahead_legalization() {
  copy_xy_to_anchor();
  init_look_ahead_legal();
  look_ahead_legal();

  update_HPWL_x();
  update_HPWL_y();
  std::cout << "Look-ahead legalization complete\n";
  std::cout << "HPWL: " << HPWLX_new + HPWLY_new << "\n";

  swap_xy_anchor();
}

void circuit_t::linear_system_solve() {
  update_max_min_node_x();
  update_max_min_node_y();
  initialize_HPWL_flags();
  /* set HPWLx_converge and HPWLy_converge to false
  update HPWLX, HPWLY, prepare for problem building */

  for (int i=0; ; i++) {
    //std::cout << "cg iteration " << i << "\n";
    if (!HPWLx_converge) {
      build_problem_b2b_x();
      add_anchor_x();
      CG_solver_x();
      update_max_min_node_x();
    }

    if (!HPWLy_converge) {
      build_problem_b2b_y();
      add_anchor_y();
      CG_solver_y();
      update_max_min_node_y();
    }
    if (HPWLx_converge && HPWLy_converge) break;
  }

  std::cout << "Linear solver complete\n";
  std::cout << "HPWL: " << HPWLX_new + HPWLY_new << "\n";
}

void circuit_t::global_placement () {
  create_grid_bins();
  write_all_terminal_grid_bins();
  write_not_all_terminal_grid_bins();
  init_update_white_space_LUT();
  int N = 1;
  for (int t = 0; t < N; t++) {
    //std::cout << t << "\n";
    look_ahead_legalization();
    std::cout << " look-ahead legalization complete\n";
    if (t == N-1) break;
    ALPHA = ALPHA * (1+t);
    linear_system_solve();
    //std::cout << "   linear solver complete\n";
  }
  std::cout << "Global Placement Complete\n";
  //std::cout << "HPWL: " << HPWLX_new + HPWLY_new << "\n";
}


void circuit_t::global_placer() {
  //std::cout << CELL_NUM << "\t" << TERMINAL_NUM << "\n";
  cg_init();
  // create empty matrix Ax, Ay, and vector bx, by in namespace cg
  initial_placement();
  global_placement();

  cg_close();
}
