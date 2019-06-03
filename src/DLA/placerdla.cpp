//
// Created by Yihang Yang on 2019-05-20.
//

#include <queue>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "placerdla.hpp"

placer_dla_t::placer_dla_t(): placer_t() {
  bin_width = 0;
  bin_height = 0;
}

placer_dla_t::placer_dla_t(double aspectRatio, double fillingRate): placer_t(aspectRatio, fillingRate) {
  bin_width = 0;
  bin_height = 0;
}

bool placer_dla_t::set_input_circuit(circuit_t *circuit) {
  if (circuit->block_list.empty()) {
    std::cout << "Error!\n";
    std::cout << "Invalid input circuit: empty block list!\n";
    return false;
  }
  if (circuit->net_list.empty()) {
    std::cout << "Error!\n";
    std::cout << "Invalid input circuit: empty net list!\n";
    return false;
  }

  _circuit = circuit;
  for (auto &&block: circuit->block_list) {
    block_dla_t block_dla;
    block_dla.retrieve_info_from_database(block);
    block_list.push_back(block_dla);
    std::cout << block_dla << "\n";
  }

  for (auto &&net: circuit->net_list) {
    net_dla_t net_dla;
    net_dla.retrieve_info_from_database(net);
    for (auto &&pin: net.pin_list) {
      int block_num = circuit->block_name_map.find(pin.get_block()->name())->second;
      block_dla_t *block_dla_ptr = &block_list[block_num];
      pin_t pin_dla(pin.x_offset(), pin.y_offset(), block_dla_ptr);
      net_dla.pin_list.push_back(pin_dla);
    }
    net_list.push_back(net_dla);
  }
  return true;
}

void placer_dla_t::add_boundary_list() {
  int max_width=0, max_height=0;
  for (auto &&block: block_list) {
    // find maximum width and maximum height
    if (!block.is_movable()) {
      continue;
    }
    if (block.width() > max_width) {
      max_width = block.width();
    }
    if (block.height() > max_height) {
      max_height = block.height();
    }
  }

  block_dla_t tmp_block;
  int multiplier = 500;
  tmp_block.set_movable(false);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(left() - tmp_block.width());
  tmp_block.set_lly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(right() - left());
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(left());
  tmp_block.set_lly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(right());
  tmp_block.set_lly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_width(top() - bottom());
  tmp_block.set_llx(right());
  tmp_block.set_lly(bottom());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(right());
  tmp_block.set_lly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(right() - left());
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(left());
  tmp_block.set_lly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(left() - tmp_block.width());
  tmp_block.set_lly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(top() - bottom());
  tmp_block.set_llx(left() - tmp_block.width());
  tmp_block.set_lly(bottom());
  boundary_list.push_back(tmp_block);
}

void placer_dla_t::initialize_bin_list(){
  // initialize the bin_list, build up the bin_list matrix
  int max_width=0, max_height=0;
  for (auto &&block: block_list) {
    // find maximum width and maximum height
    if (!block.is_movable()) {
      continue;
    }
    if (block.width() > max_width) {
      max_width = block.width();
    }
    if (block.height() > max_height) {
      max_height = block.height();
    }
  }
  size_t x_bin_num, y_bin_num;
  x_bin_num = (size_t)std::ceil((right() - left() + 2*max_width)/(double)max_width); // determine the bin numbers in x direction
  y_bin_num = (size_t)std::ceil((top() - bottom() + 2*max_height)/(double)max_height); // determine the bin numbers in y direction
  bin_width = max_width;
  bin_height = max_height;
  std::vector<bin_t> tmp_bin_column(y_bin_num);
  for (size_t x=0; x<x_bin_num; x++) {
    // bin_list[x][y] indicates the bin in the  x-th x, and y-th y bin
    bin_list.push_back(tmp_bin_column);
  }
  double Left_new = left() - (x_bin_num * bin_width - (right() - left()))/2.0;
  double Bottom_new = bottom() - (y_bin_num * bin_height - (top() - bottom()))/2.0;
  for (size_t x=0; x<bin_list.size(); x++) {
    for (size_t y=0; y<bin_list[x].size(); y++) {
      bin_list[x][y].set_left(Left_new + x*bin_width);
      bin_list[x][y].set_bottom(Bottom_new + y*bin_height);
      bin_list[x][y].set_width(bin_width);
      bin_list[x][y].set_height(bin_height);
    }
  }
}

/*
void placer_dla_t::add_neb_num(int node_num1, int node_num2, int net_size) {
  size_t neb_list_size; // neighbor number
  block_neighbor tmp_neb; // for one of its neighbor, we need to record the primary key of this neighbor and the number of wires between them
  tmp_neb.node_num = node_num2;
  tmp_neb.wire_num = 1.0 / (net_size - 1);
  neb_list_size = block_list[node_num1].neb_list.size();
  if (neb_list_size == 0) {
    block_list[node_num1].neb_list.push_back(tmp_neb);
  } else {
    for (size_t j = 0; j < block_list[node_num1].neb_list.size(); j++) {
      if (block_list[node_num1].neb_list[j].node_num != node_num2) {
        if (j == neb_list_size - 1) {
          block_list[node_num1].neb_list.push_back(tmp_neb);
          break;
        } else {
          continue;
        }
      } else {
        block_list[node_num1].neb_list[j].wire_num += tmp_neb.wire_num;
        break;
      }
    }
  }
}

void placer_dla_t::sort_neighbor_list() {
  int max_wire_node_index;
  block_neighbor tmp_neb;
  for (auto &&block: block_list) {
    for (size_t i=0; i<block.neb_list.size(); i++) {
      max_wire_node_index = i;
      for (size_t j=i+1; j<block.neb_list.size(); j++) {
        if (block.neb_list[j].wire_num > block.neb_list[max_wire_node_index].wire_num) {
          max_wire_node_index = j;
        }
      }
      tmp_neb = block.neb_list[i];
      block.neb_list[i] = block.neb_list[max_wire_node_index];
      block.neb_list[max_wire_node_index] = tmp_neb;
    }
  }
}

void placer_dla_t::update_neighbor_list() {
  // update the list of neighbor and the wire numbers connected to its neighbor, and the nets connected to this cell
  int cell1, cell2;
  size_t net_size;
  for (auto &&net: *net_list) {
    for (auto &&pin: net.pinlist) {
      cell1 = pin.pinnum;
      block_list[cell1].net.push_back(net.net_num);
      block_list[cell1].totalwire += 1;
    }
    net_size = net.pinlist.size();
    for (size_t i=0; i<net_size; i++) {
      cell1 = net.pinlist[i].pinnum;
      for (size_t j=i+1; j<net.pinlist.size(); j++) {
        cell2 = net.pinlist[j].pinnum;
        add_neb_num(cell1, cell2, net_size);
        add_neb_num(cell2, cell1, net_size);
      }
    }
  }
  sort_neighbor_list();
}

void placer_dla_t::order_node_to_place(std::queue<int> &cell_to_place){
  // creat a new cell list cell_to_place to store the sorted block_list based on the number of wires connecting to the cell
  block_dla_t tmp_block;
  std::vector<block_dla_t> tmp_node_list;
  for (auto &&block: block_list) {
    tmp_node_list.push_back(block);
  }
  int max_wire_cell;
  for (size_t i=0; i<tmp_node_list.size(); i++) {
    max_wire_cell = i;
    for (size_t j=i+1; j<tmp_node_list.size(); j++) {
      if (tmp_node_list[j].totalwire > tmp_node_list[max_wire_cell].totalwire) {
        max_wire_cell = j;
      }
    }
    tmp_block = tmp_node_list[i];
    tmp_node_list[i] = tmp_node_list[max_wire_cell];
    tmp_node_list[max_wire_cell] = tmp_block;
  }
  for (auto &&block: tmp_node_list) {
    std::cout << block.node_num << " is connected to " << block.totalwire << "\n";
    cell_to_place.push(block.node_num); // the queue of sorted cell list
  }
}

void placer_dla_t::update_bin_list(int first_node_num, std::vector<int> &cell_out_bin) {
  double left_most = bin_list[0][0].left, bottom_most = bin_list[0][0].bottom;
  int X_bin_list_size = bin_list.size(), Y_bin_list_size = bin_list[0].size();
  double right_most = left_most + X_bin_list_size * bin_width, top_most = bottom_most + Y_bin_list_size * bin_height;
  double left = block_list[first_node_num].llx(), right = block_list[first_node_num].urx();
  double bottom = block_list[first_node_num].lly(), top = block_list[first_node_num].ury();
  block_dla_t temp_large_boundry; // the larger boundries of bins
  temp_large_boundry.x0 = left_most;
  temp_large_boundry.y0 = bottom_most;
  temp_large_boundry.w = (int)(right_most - left_most);
  temp_large_boundry.h = (int)(top_most - bottom_most);

  //cout << "Cell: " << first_node_num << " is in bins: ";
  if (temp_large_boundry.overlap_area(block_list[first_node_num]) < block_list[first_node_num].area()) {
    cell_out_bin.push_back(first_node_num);
    //cout << "-1 -1; ";
  }
  int L,B,R,T; // left, bottom, right, top of the cell in which bin
  L = floor((left - left_most)/bin_width);
  B = floor((bottom - bottom_most)/bin_height);
  R = floor((right - left_most)/bin_width);
  T = floor((top - bottom_most)/bin_height);
  for (int j=B; j<=T; j++) {
    if ((j>=0)&&(j<X_bin_list_size))
    {
      for (int k=L; k<=R; k++)
      {
        if ((k>=0)&&(k<Y_bin_list_size))
        {
          bin_list[j][k].CIB.push_back(first_node_num);
          //cout << j << " " << k << "; ";
          // tempbinloc.iloc = j;
          // tempbinloc.jloc = k;
          // block_list[first_node_num].bin.push_back(tempbinloc);
        }
      }
    }
  }
  //cout << "\n";
}

bool placer_dla_t::random_release_from_boundaries(int boundary_num, block_dla_t &block) {
  switch (boundary_num) {
    case 0:
      // 2*Left boundry
      block.x0 = left() - 1*(right() - left());
      block.y0 = bottom() + std::rand()%(top() - bottom());
      break;
    case 1:
      // 2*Top boundry
      block.x0 = left() + std::rand()%(right() - left());
      block.y0 = top() + 1*(top() - bottom());
      break;
    case 2:
      // 2*Right boundry
      block.x0 = right() + 1*(right() - left());
      block.y0 = bottom() + std::rand()%(top() - bottom());
      break;
    case 3:
      // 2*Bottom boundry
      block.x0 = left() + std::rand()%(right() - left());
      block.y0 = bottom() - 1*(top() - bottom());
      break;
    default:
      return false;
  }
  return true;
}

double placer_dla_t::net_hwpl_during_dla(net_t *net) {
  double WL=0;
  int num_of_placed_cell_in_net=0, tempnetsize, tempcellNo;
  double tempcellx, tempcelly;
  double maxx = 0, minx = 0, maxy = 0, miny = 0;
  tempnetsize = net->pinlist.size();
  for (int j=0; j<tempnetsize; j++) {
    tempcellNo = net->pinlist[j].pinnum;
    if (block_list[tempcellNo].placed==1) {
      num_of_placed_cell_in_net += 1;
      //cout << Net.pin_list[j].cellNo << "\n";
      //cout << Net.pin_list[j].x_off << " " << Net.pin_list[j].y_off << "\t" << block_list[tempcellNo].left << " " << block_list[tempcellNo].bottom << "\n";
      tempcellx = block_list[tempcellNo].x0 + net->pinlist[j].xoffset;
      tempcelly = block_list[tempcellNo].y0 + net->pinlist[j].yoffset;
      if ( num_of_placed_cell_in_net == 1 ) {
        maxx = tempcellx; minx = tempcellx; maxy = tempcelly; miny = tempcelly;
      }
      else {
        if ( tempcellx > maxx ) maxx = tempcellx;
        if ( tempcellx < minx ) minx = tempcellx;
        if ( tempcelly > maxy ) maxy = tempcelly;
        if ( tempcelly < maxy ) miny = tempcelly;
      }
    }
  }
  WL = ( maxx - minx ) + ( maxy - miny );
  //cout << maxx << " " << minx << " " << maxy << " " << miny << "\n\n";
  return WL;
}

double placer_dla_t::wirelength_during_DLA(int first_node_num) {
  // support multi-pin nets
  double WL=0;
  net_t *net;
  for (size_t i=0; i<block_list[first_node_num].net.size(); i++) {
    //cout << "Net " << block_list[first_node_num].net[i] << "\n";
    net = &(net_list->at(block_list[first_node_num].net[i]));
    WL += net_hwpl_during_dla(net);
  }
  return WL;
}

int placer_dla_t::is_legal(int first_node_num, std::vector<int> &cell_out_bin) {
  // if legal, return -1, if illegal, return the cell with which this first_node_num has overlap
  double left_most = bin_list[0][0].left, bottom_most = bin_list[0][0].bottom;
  int binwidth = bin_list[0][0].width, binheight = bin_list[0][0].height;
  int Ybinlistsize = bin_list.size(), Xbinlistsize = bin_list[0].size();
  double right_most = left_most + Xbinlistsize*binwidth, top_most = bottom_most + Ybinlistsize*binheight;
  double Left = block_list[first_node_num].llx(), Right = block_list[first_node_num].urx();
  double Bottom = block_list[first_node_num].lly(), Top = block_list[first_node_num].ury();
  block_dla_t temp_large_boundry; // the larger boundries of bins
  temp_large_boundry.x0 = left_most;
  temp_large_boundry.y0 = bottom_most;
  temp_large_boundry.w = (int)(right_most - left_most);
  temp_large_boundry.h = (int)(top_most - bottom_most);

  int tempcellnum;
  if (temp_large_boundry.overlap_area(block_list[first_node_num]) < block_list[first_node_num].area()) {
    for (size_t i=0; i<cell_out_bin.size(); i++) {
      tempcellnum = cell_out_bin[i];
      if ((tempcellnum!=first_node_num)&&(block_list[first_node_num].overlap_area(block_list[tempcellnum])>=1)) return tempcellnum;
    }
  }
  int L,B,R,T; // left, bottom, right, top of the cell in which bin
  L = floor((Left - left_most)/binwidth);
  B = floor((Bottom - bottom_most)/binheight);
  R = floor((Right - left_most)/binwidth);
  T = floor((Top - bottom_most)/binheight);
  for (int j=B; j<=T; j++) {
    if ((j>=0)&&(j<Ybinlistsize)) {
      for (int k=L; k<=R; k++) {
        if ((k>=0)&&(k<Xbinlistsize)) {
          for (size_t i=0; i<bin_list[j][k].CIB.size(); i++) {
            tempcellnum = bin_list[j][k].CIB[i];
            if ((tempcellnum!=first_node_num)&&(block_list[first_node_num].overlap_area(block_list[tempcellnum])>=1)) return tempcellnum;
          }
        }
      }
    }
  }
  return -1;
}

void placer_dla_t::diffuse(int first_node_num, std::vector<int> &cell_out_bin) {
  double WL1, WL2, MW; // MW indicates minimum wirelength
  double modWL1, modWL2;
  int step;
  double center_x=(left() + right())/2.0, center_y=(bottom() + top())/2.0;
  double cell_center_x, cell_center_y, distance_to_center;
  double p1=0.00005, p2; // accept probability
  double r, T=50.0;
  block_dla_t TLUM; // Temporary Location Under Motion
  block_dla_t MWLL; // Minimum Wire-Length Location
  random_release_from_boundaries(0, block_list[first_node_num]);
  MW = wirelength_during_DLA(first_node_num);
  for (int i=0; i<4; i++) {
    // release this cell from all 4 boundries and find the location, which gives the smallest total wirelength
    random_release_from_boundaries(i, block_list[first_node_num]);
    while (true) {
      cell_center_x = block_list[first_node_num].x0;
      cell_center_y = block_list[first_node_num].y0;
      distance_to_center = fabs(cell_center_x - center_x)+fabs(cell_center_y - center_y);
      WL1 = wirelength_during_DLA(first_node_num);
      modWL1 = WL1 + distance_to_center/4.0;
      TLUM = block_list[first_node_num];
      step = (int)(WL1/100.0);
      block_list[first_node_num].random_move(std::max(step, 1));
      cell_center_x = block_list[first_node_num].x0;
      cell_center_y = block_list[first_node_num].y0;
      distance_to_center = fabs(cell_center_x - center_x)+fabs(cell_center_y - center_y);
      WL2 = wirelength_during_DLA(first_node_num);
      modWL2 = WL2 + distance_to_center/4.0;
      if (is_legal(first_node_num, cell_out_bin)==-1) {
        if (modWL2 > modWL1) {
          // if wire-length becomes larger, reject this move by some probability
          r = ((double) std::rand()/RAND_MAX);
          p2 = exp(-(modWL2-modWL1)/T);
          if (r>p2) block_list[first_node_num] = TLUM;
        }
        if (WL2<=MW)
        {
          MW = WL2;
          MWLL = block_list[first_node_num];
        }
      }
      else
      {
        block_list[first_node_num] = TLUM;
        r = ((double)std::rand()/RAND_MAX);
        if (r<=p1) break;
      }
    }
  }
  block_list[first_node_num] = MWLL;

  //cout << first_node_num << " " << MWLL.cellNo << "\n";
  //for (int j=0; j<block_list.size(); j++)
  //{
    //if (block_list[j].cellNo == -1)
    //{
      //cout << j << " errordiffuse\n";
      //exit(1);
    //}
  //}
  update_bin_list(first_node_num, cell_out_bin);
}

bool placer_dla_t::DLA() {
  update_neighbor_list();
  std::queue<int> cell_to_place;
  std::vector<int> cell_out_bin; // cells which are out of bins
  order_node_to_place(cell_to_place);

  std::queue<int> Q_place;
  int first_node_num, tmp_num;
  first_node_num = cell_to_place.front();
  cell_to_place.pop();
  Q_place.push(first_node_num);
  block_list[first_node_num].queued = true;
  int num_of_node_placed = 0;
  while (!Q_place.empty()) {
    std::cout << Q_place.size() << "\n";
    first_node_num = Q_place.front();
    Q_place.pop();
    std::cout << "Placing cell " << first_node_num << "\n";
    for (size_t j=0; j<block_list[first_node_num].neb_list.size(); j++) {
      tmp_num = block_list[first_node_num].neb_list[j].node_num;
      if (block_list[tmp_num].queued){
        continue;
      } else {
        block_list[tmp_num].queued = true;
        Q_place.push(tmp_num);
      }
    }
    block_list[first_node_num].placed = true;
    if (num_of_node_placed == 0) {
      block_list[first_node_num].x0 = (left() + right())/2.0;
      block_list[first_node_num].y0 = (bottom() + top())/2.0;
      update_bin_list(first_node_num, cell_out_bin);
    }
    else {
      diffuse(first_node_num, cell_out_bin);
    }
    num_of_node_placed += 1;
    //drawplaced(block_list, net_list, boundries);
    //if (numofcellplaced==2) break;
    if (Q_place.empty()) {
      while (!cell_to_place.empty()) {
        first_node_num = cell_to_place.front();
        if (!block_list[first_node_num].queued) {
          Q_place.push(first_node_num);
          break;
        } else {
          cell_to_place.pop();
        }
      }
    }
  }
  //std::cout << "Total WireLength after placement is " <<  TotalWireLength(block_list, net_list) << "\n";
  return true;
}
*/

bool placer_dla_t::start_placement() {
  add_boundary_list();
  initialize_bin_list();
  draw_bin_list();
  /*
  DLA();
   */
  return true;
}

bool placer_dla_t::draw_bin_list(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&bin_column: bin_list) {
    for (auto &&bin: bin_column) {
      ost << "rectangle('Position',[" << bin.left() << " " << bin.bottom() << " " << bin.width() << " " << bin.height() << "],'LineWidth',1)\n";
    }
  }
  ost << "rectangle('Position',[" << left() << " " << bottom() << " " << right() - left() << " " << top() - bottom() << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}

bool placer_dla_t::draw_block_net_list(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&block: block_list) {
    ost << "rectangle('Position',[" << block.llx() << " " << block.lly() << " " << block.width() << " " << block.height() << "], 'LineWidth',3)\n";
  }
  int cell1, cell2;
  int x1, x2, y1, y2;
  int xoff1, xoff2, yoff1, yoff2;
  /*
  for (int i=0; i<net_list.size(); i++) {
    cell1 = net_list[i].pin_list[0].cellNo;
    cell2 = net_list[i].pin_list[1].cellNo;
    x1 = block_list[cell1].llx(); y1 = block_list[cell1].lly();
    x2 = block_list[cell2].llx(); y2 = block_list[cell2].lly();
    xoff1 = net_list[i].pin_list[0].x_offset(); yoff1 = net_list[i].pin_list[0].y_offset();
    xoff2 = net_list[i].pin_list[1].x_offset(); yoff2 = net_list[i].pin_list[1].y_offset();
    ost << "line([" << x1+xoff1 << "," << x2+xoff2 << "],[" << y1+yoff1 << "," << y2+yoff2 << "],'lineWidth', 2)\n";
    //ost << "line([" << x1+xoff1 << "," << x2+xoff2 << "],[" << y1+yoff1 << "," << y2+yoff2 << "],'Color',[.5 .5 .5],'lineWidth', 2)\n";
  }
   */
  ost << "rectangle('Position',[" << left() << " " << bottom() << " " << right() - left() << " " << top() - bottom() << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}

bool placer_dla_t::draw_placed_blocks(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&block: block_list) {
    if (block.is_placed()) {
      ost << "rectangle('Position',[" << block.llx() << " " << block.lly() << " " << block.width() << " " << block.height() << "], 'LineWidth',3)%" << block.num() << "\n";
    }
  }
  int cell1, cell2, tempnetsize;
  int x1, x2, y1, y2;
  int xoff1, xoff2, yoff1, yoff2;
  /*
  for (int i=0; i<net_list.size(); i++)
  {
    tempnetsize = net_list[i].pin_list.size();
    for (int j=0; j<tempnetsize; j++)
    {
      for (int k=j+1; k<tempnetsize; k++)
      {
        cell1 = net_list[i].pin_list[j].cellNo;
        cell2 = net_list[i].pin_list[k].cellNo;
        x1 = block_list[cell1].llx(); y1 = block_list[cell1].lly();
        x2 = block_list[cell2].llx(); y2 = block_list[cell2].lly();
        xoff1 = net_list[i].pin_list[j].x_offset(); yoff1 = net_list[i].pin_list[j].y_offset();
        xoff2 = net_list[i].pin_list[k].x_offset(); yoff2 = net_list[i].pin_list[k].y_offset();
        ost << "line([" << x1+xoff1 << "," << x2+xoff2 << "],[" << y1+yoff1 << "," << y2+yoff2 << "],'lineWidth', " << 3*1.0/(tempnetsize-1) << ")\n";
      }
    }
  }
   */
  ost << "rectangle('Position',[" << left() << " " << bottom() << " " << right() - left() << " " << top() - bottom() << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}

bool placer_dla_t::output_result(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&block: block_list) {
    ost << "\t" << block.name() << "\t" << block.llx() << "\t" << block.lly() << "\t : N\n";
  }
  ost.close();

  return true;
}

placer_dla_t::~placer_dla_t() {

}
