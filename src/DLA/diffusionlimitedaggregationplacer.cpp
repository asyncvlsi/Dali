//
// Created by Yihang Yang on 2019-05-20.
//

#include <queue>
#include <vector>
#include "diffusionlimitedaggregationplacer.hpp"

diffusion_limited_aggregation_placer::diffusion_limited_aggregation_placer() {
  circuit = nullptr;
}

diffusion_limited_aggregation_placer::diffusion_limited_aggregation_placer(circuit_t &input_circuit) {
  circuit = &input_circuit;
}

void diffusion_limited_aggregation_placer::addnebnum(std::vector<node_t> &celllist, int cell1, int cell2, int netsize) {
  int neblistsize; // neighbor number
  cell_neighbor tempneb; // for one of its neighbor, the primay key of this neighbor and the number of wires between them
  tempneb.cellNum = cell2;
  tempneb.wireNum = 1.0/(netsize-1);
  neblistsize = celllist[cell1].neblist.size();
  if (neblistsize == 0)
  {
    celllist[cell1].neblist.push_back(tempneb);
  }
  else
  {
    for (int j=0; j<celllist[cell1].neblist.size(); j++)
    {
      if (celllist[cell1].neblist[j].cellNum!=cell2)
      {
        if (j==neblistsize-1)
        {
          celllist[cell1].neblist.push_back(tempneb);
          break;
        }
        else continue;
      }
      else
      {
        celllist[cell1].neblist[j].wireNum += tempneb.wireNum;
        break;
      }
    }
  }
}

void diffusion_limited_aggregation_placer::update_neighbor_list(std::vector<node_t> &celllist, std::vector<net_t> &NetList) {
  // update the list of neighbor and the wire numbers connected to its neighbor, and the nets connected to this cell
  int cell1, cell2;
  int tempnetsize;
  for (int i=0; i<NetList.size(); i++)
  {
    tempnetsize = NetList[i].pinlist.size();
    for (int j=0; j<tempnetsize; j++)
    {
      cell1 = NetList[i].pinlist[j].pinnum;
      celllist[cell1].net.push_back(i);
      celllist[cell1].totalwire += 1;
    }
    for (int j=0; j<tempnetsize; j++)
    {
      cell1 = NetList[i].pinlist[j].pinnum;
      for (int k=j+1; k<NetList[i].pinlist.size(); k++)
      {
        cell2 = NetList[i].pinlist[k].pinnum;
        addnebnum(celllist, cell1, cell2, tempnetsize);
        addnebnum(celllist, cell2, cell1, tempnetsize);
      }
    }
  }
  sort_neighbor_list(celllist);
}


bool diffusion_limited_aggregation_placer::start_place() {
  update_neighbor_list(celllist, NetList);
  std::queue<int> cell_to_place;
  std::vector<int> cell_out_bin; // cells which are out of bins
  celltoplacelist(cell_to_place, celllist);
  int Left=boundries[0], Bottom=boundries[1], Right=boundries[2], Top=boundries[3];
  std::queue<int> Q_place;
  int firstcell, tempnum;
  firstcell = cell_to_place.front(); cell_to_place.pop(); Q_place.push(firstcell);
  celllist[firstcell].queued = 1;
  int numofcellplaced = 0;
  //cout << Left << " " << Right << " " << Bottom << " " << Top << "\n";
  while (Q_place.size()>0)
  {
    //cout << Q_place.size() << "\n";
    firstcell = Q_place.front(); Q_place.pop();
    std::cout << "Placing cell " << firstcell << "\n";
    for (int j=0; j<celllist[firstcell].neblist.size(); j++)
    {
      tempnum = celllist[firstcell].neblist[j].cellNum;
      if (celllist[tempnum].queued == 1) continue;
      else
      {
        celllist[tempnum].queued =1;
        Q_place.push(tempnum);
      }
    }
    celllist[firstcell].placed = 1;
    if (numofcellplaced==0)
    {
      celllist[firstcell].left = (Left+Right)/2 - celllist[firstcell].width/2;
      celllist[firstcell].bottom = (Bottom+Top)/2 - celllist[firstcell].height/2;
      update_binlist(firstcell, cell_out_bin, binlist, celllist);
    }
    else
    {
      diffuse(firstcell, cell_out_bin, binlist, celllist, NetList, boundries);
    }
    numofcellplaced += 1;
    //drawplaced(celllist, NetList, boundries);
    //if (numofcellplaced==2) break;
    if (Q_place.size()==0)
    {
      while (cell_to_place.size()>0)
      {
        firstcell = cell_to_place.front();
        if (celllist[firstcell].queued==0)
        {
          Q_place.push(firstcell);
          break;
        }
        else cell_to_place.pop();
      }
    }
  }
  std::cout << "Total WireLength after placement is " <<  TotalWireLength(celllist, NetList) << "\n";
}