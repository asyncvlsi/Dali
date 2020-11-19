//
// Created by yihang on 11/12/20.
//

#include "si2lefdef.h"

#include "defrReader.hpp"
#include "lefrReader.hpp"

int getLefUnits(lefrCallbackType_e type, lefiUnits *units, lefiUserData userData) {
  if (type != lefrUnitsCbkType) {
    std::cout << "Type is not lefrUnitsCbkType!" << std::endl;
    exit(2);
  }
  if (units->hasDatabase()) {
    Circuit &circuit = *((Circuit *) userData);
    double number = units->databaseNumber();
    circuit.setDatabaseMicron(int(number));
    std::cout << "DATABASE MICRONS " << units->databaseNumber() << std::endl;
  } else {
    Assert(false, "No DATABASE MICRONS provided in the UNITS section?");
  }
  return 0;
}

int getLefManufacturingGrid(lefrCallbackType_e type, double number, lefiUserData userData) {
  if (type != lefrManufacturingCbkType) {
    std::cout << "Type is not lefrPinCbkType!" << std::endl;
    exit(2);
  }
  Circuit &circuit = *((Circuit *) userData);
  circuit.setManufacturingGrid(number);
  std::cout << "MANUFACTURINGGRID " << number << std::endl;

  return 0;
}

int getLefLayers(lefrCallbackType_e type, lefiLayer *layer, lefiUserData userData) {
  if (type != lefrLayerCbkType) {
    std::cout << "Type is not lefrLayerCbkType!" << std::endl;
    exit(2);
  }

  if (strcmp(layer->type(), "ROUTING") == 0) { // routing layer
    std::string metal_layer_name(layer->name());
    std::cout << metal_layer_name << "\n";
    double min_width = layer->width();
    if (layer->hasMinwidth()) {
      min_width = layer->minwidth();
    }
    double min_spacing = layer->spacing(0);
    std::cout << min_width << "  " << min_spacing << "  ";
    std::string str_direct(layer->direction());
    std::cout << str_direct << "\n";
    MetalDirection direct = StrToMetalDirection(str_direct);
    double min_area = 0;
    if (layer->hasArea()) {
      min_area = layer->area();
      std::cout << min_area;
    }
    Circuit &circuit = *((Circuit *) userData);
    circuit.AddMetalLayer(metal_layer_name,
                          min_width,
                          min_spacing,
                          min_area,
                          min_width + min_spacing,
                          min_width + min_spacing,
                          direct);
    //std::cout << "\n";
  }

  return 0;
}

int siteCB(lefrCallbackType_e type, lefiSite *site, lefiUserData userData) {
  if (type != lefrSiteCbkType) {
    std::cout << "Type is not lefrSiteCbkType!" << std::endl;
    exit(2);
  }
  if (site->lefiSite::hasSize()) {
    Circuit &circuit = *((Circuit *) userData);
    circuit.setGridValue(site->sizeX(), site->sizeY());
    //std::cout << "SITE SIZE " << site->lefiSite::sizeX() << "  " << site->lefiSite::sizeY() << "\n";
    //std::cout << circuit.GridValueX() << "  " << circuit.GridValueY() << "\n";
  } else {
    Assert(false, "SITE SIZE information not provided");
  }
  return 0;
}

int macroBeginCB(lefrCallbackType_e type, const char *macroName, lefiUserData userData) {
  if (type != lefrMacroBeginCbkType) {
    std::cout << "Type is not lefrMacroBeginCbkType!" << std::endl;
    exit(2);
  }
  Circuit &circuit = *((Circuit *) userData);
  std::string tmpMacroName(macroName);
  BlockType *&tmpMacro = circuit.getTechRef().last_blk_type_;
  Assert(tmpMacro == nullptr, "Expect an nullptr");
  if (tmpMacroName.find("welltap") != std::string::npos) {
    tmpMacro = circuit.AddWellTapBlockType(tmpMacroName, 0, 0);
  } else {
    tmpMacro = circuit.AddBlockType(tmpMacroName, 0, 0);
  }
  //std::cout << tmpMacro->Name() << "\n";
  return 0;
}

int getLefPins(lefrCallbackType_e type, lefiPin *pin, lefiUserData userData) {
  if (type != lefrPinCbkType) {
    std::cout << "Type is not lefrPinCbkType!" << std::endl;
    exit(2);
  }
  Circuit &circuit = *((Circuit *) userData);
  Tech &tech = circuit.getTechRef();
  BlockType *&tmpMacro = circuit.getTechRef().last_blk_type_;
  Assert(tmpMacro != nullptr, "tmp macro cannot be a nullptr when setting macro pin info");
  std::string tmpMacroName = circuit.getTechRef().last_blk_type_->Name();

  //std::cout << "This is a lef pin callback\n";
  std::string pin_name(pin->name());
  //std::cout << "  " << pin_name << "\n";
  //if (pin_name == "Vdd" || pin_name == "GND") continue;
  Assert(pin->numPorts() > 0, "No physical pins, Macro: " + tmpMacroName + ", pin: " + pin_name);
  bool is_input = true;
  if (strcmp(pin->direction(), "INPUT") == 0) {
    is_input = true;
  } else if (strcmp(pin->direction(), "OUTPUT") == 0) {
    is_input = false;
  } else {
    is_input = false;
    //Assert(false, "Unsupported terminal IO type\n");
  }
  Pin *new_pin = circuit.AddBlkTypePin(tmpMacro, pin_name, is_input);
  int numPorts = pin->numPorts();
  for (int i = 0; i < numPorts; ++i) {
    int numItems = pin->port(i)->numItems();
    for (int j = 0; j < numItems; ++j) {
      int itemType = pin->port(i)->itemType(j);
      if (itemType == lefiGeomRectE) {
        double llx = pin->port(i)->getRect(j)->xl / circuit.GridValueX();
        double urx = pin->port(i)->getRect(j)->xh / circuit.GridValueX();
        double lly = pin->port(i)->getRect(j)->yl / circuit.GridValueY();
        double ury = pin->port(i)->getRect(j)->yh / circuit.GridValueY();
        //std::cout << "  PORT: " << llx << "  " << lly << "  " << urx << "  " << ury << "  " << "\n";
        circuit.AddBlkTypePinRect(new_pin, llx, lly, urx, ury);
      }
    }
  }

  return 0;
}

int getLefMacros(lefrCallbackType_e type, lefiMacro *macro, lefiUserData userData) {
  /****
   * This callback function is called after Pin and OBS, but before macrosEND.
   * ****/
  if ((type != lefrMacroCbkType)) {
    std::cout << "Type is not lefrMacroCbkType!" << std::endl;
    exit(2);
  }

  double originX = macro->originX();
  double originY = macro->originY();
  double sizeX = macro->sizeX();
  double sizeY = macro->sizeY();

  Assert(originX == 0 && originY == 0, "Only support originX and originY equal 0");
  Circuit &circuit = *((Circuit *) userData);
  BlockType *&tmpMacro = circuit.getTechRef().last_blk_type_;
  circuit.SetBlockTypeSize(tmpMacro, sizeX, sizeY);

  //std::cout << "MACRO " << tmpMacro->Name() << "\n"
  //          << "  ORIGIN " << originX << " " << originY << " ;\n"
  //          << "  SIZE   " << sizeX << " " << sizeY << " ;\n";

  return 0;
}

int macroEndCB(lefrCallbackType_e type, const char *macroName, lefiUserData userData) {
  if (type != lefrMacroEndCbkType) {
    std::cout << "Type is not lefrMacroEndCbkType!" << std::endl;
    exit(2);
  }
  Circuit &circuit = *((Circuit *) userData);
  BlockType *&tmpMacro = circuit.getTechRef().last_blk_type_;
  Assert(tmpMacro != nullptr, "A MACRO end before begin?");
  //std::cout << "END " << tmpMacro->Name() << "\n";
  tmpMacro = nullptr;

  return 0;
}

void readLef(std::string &lefFileName, Circuit &circuit) {
  //lefrInit();
  lefrInitSession(1);

  lefrSetUserData((lefiUserData) &circuit);

  lefrSetUnitsCbk(getLefUnits);
  lefrSetManufacturingCbk(getLefManufacturingGrid);
  lefrSetLayerCbk(getLefLayers);
  lefrSetSiteCbk(siteCB);

  lefrSetMacroBeginCbk(macroBeginCB);
  lefrSetPinCbk(getLefPins);
  lefrSetMacroCbk(getLefMacros);
  lefrSetMacroEndCbk(macroEndCB);

  FILE *f;
  if ((f = fopen(lefFileName.c_str(), "r")) == nullptr) {
    std::cout << "Couldn't open lef file" << std::endl;
    exit(2);
  }

  int res = lefrRead(f, lefFileName.c_str(), (lefiUserData) &circuit);
  if (res != 0) {
    std::cout << "LEF parser returns an error!" << std::endl;
    exit(2);
  }
  fclose(f);

  lefrClear();
}

int designCB(defrCallbackType_e type, const char *designName, defiUserData userData) {
  if (type != defrDesignStartCbkType) {
    std::cout << "Type is not defrDesignStartCbkType!" << std::endl;
    exit(2);
  }
  if (!designName || !*designName) {
    Assert(false, "Design name is null, terminate parsing.\n");
  }
  Circuit &circuit = *((Circuit *) userData);
  circuit.getDesignRef().name_ = std::string(designName);
  std::cout << circuit.getDesignRef().name_ << "\n";
  return 0;
}

int getDefUnits(defrCallbackType_e type, double number, defiUserData userData) {
  if (type != defrUnitsCbkType) {
    std::cout << "Type is not defrUnitsCbkType!" << std::endl;
    exit(2);
  }
  Circuit &circuit = *((Circuit *) userData);
  circuit.setUnitsDistanceMicrons(int(number));
  std::cout << "UNITS DISTANCE MICRONS " << circuit.DistanceMicrons() << " ;" << std::endl;
  return 0;
}

int getDefDieArea(defrCallbackType_e type, defiBox *box, defiUserData userData) {
  if (type != defrDieAreaCbkType) {
    std::cout << "Type is not defrDieAreaCbkType!" << std::endl;
    exit(1);
  }
  Circuit &circuit = *((Circuit *) userData);
  if (!circuit.getDesignRef().die_area_set_) {
    circuit.setDieArea(box->xl(), box->yl(), box->xh(), box->yh());
    std::cout << circuit.RegionLLX() << "  " << circuit.RegionLLY() << "  " << circuit.RegionURX() << "  "
              << circuit.RegionURY() << "\n";
  } else {
    int components_count = circuit.getDesignRef().blk_count_;
    int pins_count = circuit.getDesignRef().iopin_count_;
    int nets_count = circuit.getDesignRef().net_count_;
    circuit.setListCapacity(components_count, pins_count, nets_count);
    std::cout << "components count: " << components_count << "\n"
              << "pins count:        " << pins_count << "\n"
              << "nets count:       " << nets_count << "\n";
  }
  return 0;
}

int countNumberCB(defrCallbackType_e type, int num, defiUserData userData) {
  std::string name;
  Circuit &circuit = *((Circuit *) userData);
  switch (type) {
    case defrComponentStartCbkType : {
      name = "COMPONENTS";
      circuit.getDesignRef().blk_count_ = num;
      break;
    }
    case defrStartPinsCbkType : {
      name = "PINS";
      circuit.getDesignRef().iopin_count_ = num;
      break;
    }
    case defrNetStartCbkType : {
      name = "NETS";
      circuit.getDesignRef().net_count_ = num;
      break;
    }
    default : {
      name = "BOGUS";
      Assert(false, "Unsupported callback types");
    }
  }
  return 0;
}

int getDefTracks(defrCallbackType_e type, defiTrack *track, defiUserData userData) {
  if (type != defrTrackCbkType) {
    std::cout << "Type is not defrTrackCbkType!" << std::endl;
    exit(2);
  }

  return 0;
}

int getDefComponents(defrCallbackType_e type, defiComponent *comp, defiUserData userData) {
  if (type != defrComponentCbkType) {
    std::cout << "Type is not defrComponentCbkType!" << std::endl;
    exit(1);
  }

  Circuit &circuit = *((Circuit *) userData);
  Assert(circuit.getDesignRef().die_area_set_, "Die area not provided, cannot proceed");

  if (!comp->id() || !*comp->id()) {
    Assert(false, "Component name is null, terminate parsing.\n");
  }
  std::string blk_name(comp->id());
  if (!comp->name() || !*comp->name()) {
    Assert(false, "Component macro name is null, terminate parsing.\n");
  }
  std::string blk_type_name(comp->name());
  //std::cout << "ID: " << comp->id() << " NAME: " << comp->name() << "\n";
  //std::cout << circuit.GridValueX() << "  " << circuit.GridValueY() << "\n";
  //std::cout << comp->placementX() << "  " << comp->placementY() << "\n";
  int llx_int =
      (int) std::round(comp->placementX() / circuit.GridValueX() / circuit.DistanceMicrons());
  int lly_int =
      (int) std::round(comp->placementY() / circuit.GridValueY() / circuit.DistanceMicrons());
  //std::cout << llx_int << "  " << lly_int << "\n";
  std::string orient(std::string(comp->placementOrientStr()));
  PlaceStatus place_status = UNPLACED_;
  if (comp->isFixed()) {
    place_status = FIXED_;
  } else if (comp->isCover()) {
    place_status = COVER_;
  } else if (comp->isPlaced()) {
    place_status = PLACED_;
  } else if (comp->isUnplaced()) {
    place_status = UNPLACED_;
    llx_int = 0;
    lly_int = 0;
  } else {
    llx_int = 0;
    lly_int = 0;
  }
  circuit.AddBlock(blk_name, blk_type_name, llx_int, lly_int, place_status, StrToOrient(orient));

  return 0;
}

int getDefIOPins(defrCallbackType_e type, defiPin *pin, defiUserData userData) {
  if (type != defrPinCbkType) {
    std::cout << "Type is not defrPinCbkType!" << std::endl;
    exit(1);
  }

  Circuit &circuit = *((Circuit *) userData);
  std::string iopin_name(pin->pinName());
  bool is_loc_set = false;
  int iopin_x = 0;
  int iopin_y = 0;
  PlaceStatus place_status;
  if (pin->hasPlacement()) {
    if (pin->isFixed()) {
      place_status = FIXED_;
      iopin_x =
          (int) std::round(pin->placementX() / circuit.GridValueX() / circuit.DistanceMicrons());
      iopin_y =
          (int) std::round(pin->placementY() / circuit.GridValueY() / circuit.DistanceMicrons());
      is_loc_set = true;
    } else if (pin->isCover()) {
      place_status = COVER_;
      iopin_x =
          (int) std::round(pin->placementX() / circuit.GridValueX() / circuit.DistanceMicrons());
      iopin_y =
          (int) std::round(pin->placementY() / circuit.GridValueY() / circuit.DistanceMicrons());
      is_loc_set = true;
    } else if (pin->isPlaced()) {
      place_status = PLACED_;
      iopin_x =
          (int) std::round(pin->placementX() / circuit.GridValueX() / circuit.DistanceMicrons());
      iopin_y =
          (int) std::round(pin->placementY() / circuit.GridValueY() / circuit.DistanceMicrons());
      is_loc_set = true;
    } else if (pin->isUnplaced()) {
      place_status = UNPLACED_;
    } else {
      place_status = UNPLACED_;
    }
  }
  std::string str_sig_use;
  if (pin->hasUse()) {
    str_sig_use = std::string(pin->use());
  }
  Assert(!str_sig_use.empty(), "Please provide IOPIN use");
  SignalUse sig_use = StrToSignalUse(str_sig_use);

  std::string str_sig_dir;
  if (pin->hasDirection()) {
    str_sig_dir = std::string(pin->direction());
  }
  SignalDirection sig_dir = StrToSignalDirection(str_sig_dir);

  if (is_loc_set) {
    circuit.AddIOPin(iopin_name, PLACED_, sig_use, sig_dir, iopin_x, iopin_y);
  } else {
    circuit.AddIOPin(iopin_name, UNPLACED_, sig_use, sig_dir);
  }

  return 0;

}

int getDefNets(defrCallbackType_e type, defiNet *net, defiUserData userData) {
  if (type != defrNetCbkType) {
    std::cout << "Type is not defrNetCbkType!" << std::endl;
    exit(2);
  }

  Circuit &circuit = *((Circuit *) userData);
  std::string net_name(net->name());
  int net_capacity = int(net->numConnections());
  circuit.AddNet(net_name, net_capacity, circuit.getDesignRef().normal_signal_weight);

  //TO-DO: IOPIN in nets not added
  //std::cout << "- " << net_name;
  for (int i = 0; i < net->numConnections(); i++) {
    std::string blk_name(net->instance(i));
    std::string pin_name(net->pin(i));
    circuit.AddBlkPinToNet(blk_name, pin_name, net_name);
    //std::cout << " ( " << blk_name << ", " << pin_name << " ) ";
  }
  //std::cout << "\n";

  return 0;
}

int getDefVoid(defrCallbackType_e type, void *dummy, defiUserData userData) {
  if (type != defrDesignEndCbkType) {
    std::cout << "Type is not defrDesignEndCbkType!" << std::endl;
    exit(2);
  }
  std::cout << "END of DEF\n";
  return 0;
}

void readDef(std::string &defFileName, Circuit &circuit) {
  defrInit();
  defrReset();

  defrInitSession(1);
  defrSetUserData((defiUserData) &circuit);
  defrSetDesignCbk(designCB);
  defrSetUnitsCbk(getDefUnits);
  defrSetDieAreaCbk(getDefDieArea);
  defrSetComponentStartCbk(countNumberCB);
  defrSetNetStartCbk(countNumberCB);
  defrSetStartPinsCbk(countNumberCB);

  FILE *f;
  if ((f = fopen(defFileName.c_str(), "r")) == nullptr) {
    std::cout << "Couldn't open def file" << std::endl;
    exit(2);
  }

  int res = defrRead(f, defFileName.c_str(), (defiUserData) &circuit, 1);
  if (res != 0) {
    std::cout << "DEF parser returns an error in round 1!" << std::endl;
    exit(2);
  }
  fclose(f);

  defrInitSession(2);
  defrSetUserData((defiUserData) &circuit);
  //defrSetDesignCbk(designCB);
  //defrSetUnitsCbk(getDefUnits);
  defrSetDieAreaCbk(getDefDieArea);
  defrSetComponentCbk(getDefComponents);

  defrSetPinCbk(getDefIOPins);

  defrSetAddPathToNet();
  defrSetNetCbk(getDefNets);

  //defrSetDesignEndCbk(getDefVoid);
  if ((f = fopen(defFileName.c_str(), "r")) == nullptr) {
    std::cout << "Couldn't open def file" << std::endl;
    exit(2);
  }
  res = defrRead(f, defFileName.c_str(), (defiUserData) &circuit, 1);
  if (res != 0) {
    std::cout << "DEF parser returns an error in round 2!" << std::endl;
    exit(2);
  }
  fclose(f);

  //numPins = readPinCnt;

  defrClear();
}
