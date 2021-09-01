//
// Created by Yihang Yang on 11/12/20.
//

#include "si2lefdef.h"

#include <defrReader.hpp>
#include <lefrReader.hpp>

namespace dali {

int getLefUnits(lefrCallbackType_e type, lefiUnits *units, lefiUserData userData) {
  if (type != lefrUnitsCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not lefrUnitsCbkType!" << std::endl;
    exit(2);
  }
  if (units->hasDatabase()) {
    Circuit &circuit = *((Circuit *) userData);
    double number = units->databaseNumber();
    circuit.setDatabaseMicron(int(number));
    BOOST_LOG_TRIVIAL(trace) << "DATABASE MICRONS " << units->databaseNumber() << std::endl;
  } else {
    DaliExpects(false, "No DATABASE MICRONS provided in the UNITS section?");
  }
  return 0;
}

int getLefManufacturingGrid(lefrCallbackType_e type, double number, lefiUserData userData) {
  if (type != lefrManufacturingCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not lefrPinCbkType!" << std::endl;
    exit(2);
  }
  Circuit &circuit = *((Circuit *) userData);
  circuit.setManufacturingGrid(number);
  BOOST_LOG_TRIVIAL(trace) << "MANUFACTURINGGRID " << number << std::endl;

  return 0;
}

int getLefLayers(lefrCallbackType_e type, lefiLayer *layer, lefiUserData userData) {
  if (type != lefrLayerCbkType) {
    BOOST_LOG_TRIVIAL(fatal) << "Type is not lefrLayerCbkType!" << std::endl;
    exit(2);
  }

  if (strcmp(layer->type(), "ROUTING") == 0) { // routing layer
    std::string metal_layer_name(layer->name());
    BOOST_LOG_TRIVIAL(trace) << metal_layer_name << "\n";
    double min_width = layer->width();
    if (layer->hasMinwidth()) {
      min_width = layer->minwidth();
    }
    DaliExpects(min_width > 0, "layer min-width cannot be correctly identified: " + metal_layer_name);
    double min_spacing = -1.0;
    if (layer->numSpacing() > 0) {
      min_spacing = layer->spacing(0);
    } else if (layer->numSpacingTable() > 0) {
      auto spTable = layer->spacingTable(0);
      if (spTable->isParallel() == 1) {
        auto parallel = spTable->parallel();
        //std::cout << "  SPACINGTABLE\n";
        //std::cout << "  PARALLELRUNLENGTH\n\t";
        //for (int j = 0; j < parallel->numLength(); ++j) {
        //  std::cout << parallel->length(j) << "\t";
        //}
        //std::cout << "\n";
        //for (int j = 0; j < parallel->numWidth(); ++j) {
        //  std::cout << parallel->width(j) << "\t";
        //  for (int k = 0; k < parallel->numLength(); ++k) {
        //    std::cout << parallel->widthSpacing(j, k) << "\t";
        //  }
        //  std::cout << "\n";
        //}
        BOOST_LOG_TRIVIAL(trace) << "Using min-spacing from the spacing table\n";
        DaliExpects(parallel->numLength()>0 && parallel->numWidth()>0, "Spacing table is too small to obtain the min-spacing");
        min_spacing = parallel->widthSpacing(0, 0);
      } else {
        std::cout << "unsupported spacing table!\n";
      }
    } else {
      DaliExpects(false, "layer min-spacing not specified: " + metal_layer_name);
    }
    DaliExpects(min_spacing > 0, "Negative min-spacing?");

    BOOST_LOG_TRIVIAL(trace) << min_width << "  " << min_spacing << "  ";
    std::string str_direct(layer->direction());
    BOOST_LOG_TRIVIAL(trace) << str_direct << "\n";
    MetalDirection direct = StrToMetalDirection(str_direct);
    double min_area = 0;
    if (layer->hasArea()) {
      min_area = layer->area();
      BOOST_LOG_TRIVIAL(trace) << min_area;
    }
    Circuit &circuit = *((Circuit *) userData);
    circuit.AddMetalLayer(metal_layer_name,
                          min_width,
                          min_spacing,
                          min_area,
                          min_width + min_spacing,
                          min_width + min_spacing,
                          direct);
    //BOOST_LOG_TRIVIAL(info)   << "\n";
  }

  return 0;
}

int siteCB(lefrCallbackType_e type, lefiSite *site, lefiUserData userData) {
  if (type != lefrSiteCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not lefrSiteCbkType!" << std::endl;
    exit(2);
  }
  if (site->lefiSite::hasSize()) {
    Circuit &circuit = *((Circuit *) userData);
    circuit.SetGridValue(site->sizeX(), site->sizeY());
    circuit.setRowHeight(site->sizeY());
    BOOST_LOG_TRIVIAL(info)   << "SITE SIZE " << site->lefiSite::sizeX() << "  " << site->lefiSite::sizeY() << "\n";
    BOOST_LOG_TRIVIAL(info)   << circuit.GridValueX() << "  " << circuit.GridValueY() << "\n";
  } else {
    DaliExpects(false, "SITE SIZE information not provided");
  }
  return 0;
}

int macroBeginCB(lefrCallbackType_e type, const char *macroName, lefiUserData userData) {
  if (type != lefrMacroBeginCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not lefrMacroBeginCbkType!" << std::endl;
    exit(2);
  }
  Circuit &circuit = *((Circuit *) userData);
  std::string tmpMacroName(macroName);
  auto &tmpMacro = circuit.getTechRef().last_blk_type_;
  DaliExpects(tmpMacro == nullptr, "Expect an nullptr");
  if (tmpMacroName.find("welltap") != std::string::npos) {
    circuit.getTechRef().last_blk_type_ = circuit.AddWellTapBlockType(tmpMacroName, 0, 0);
  } else {
    circuit.getTechRef().last_blk_type_ = circuit.AddBlockType(tmpMacroName, 0, 0);
  }
  //BOOST_LOG_TRIVIAL(info)   << tmpMacro->Name() << "\n";
  return 0;
}

int getLefPins(lefrCallbackType_e type, lefiPin *pin, lefiUserData userData) {
  if (type != lefrPinCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not lefrPinCbkType!" << std::endl;
    exit(2);
  }
  Circuit &circuit = *((Circuit *) userData);
  Tech &tech = circuit.getTechRef();
  auto &tmpMacro = circuit.getTechRef().last_blk_type_;
  DaliExpects(tmpMacro != nullptr, "tmp macro cannot be a nullptr when setting macro pin info");
  std::string tmpMacroName = circuit.getTechRef().last_blk_type_->Name();

  //BOOST_LOG_TRIVIAL(info)   << "This is a lef pin callback\n";
  std::string pin_name(pin->name());
  //BOOST_LOG_TRIVIAL(info)   << "  " << pin_name << "\n";
  //if (pin_name == "Vdd" || pin_name == "GND") continue;
  DaliExpects(pin->numPorts() > 0, "No physical pins, Macro: " + tmpMacroName + ", pin: " + pin_name);
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
        //BOOST_LOG_TRIVIAL(info)   << "  PORT: " << lx << "  " << lly << "  " << urx << "  " << ury << "  " << "\n";
        new_pin->AddRectOnly(llx, lly, urx, ury);
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
    BOOST_LOG_TRIVIAL(info) << "Type is not lefrMacroCbkType!" << std::endl;
    exit(2);
  }

  double originX = macro->originX();
  double originY = macro->originY();
  double sizeX = macro->sizeX();
  double sizeY = macro->sizeY();

  DaliExpects(originX == 0 && originY == 0, "Only support originX and originY equal 0");
  Circuit &circuit = *((Circuit *) userData);
  auto &tmpMacro = circuit.getTechRef().last_blk_type_;
  circuit.SetBlockTypeSize(tmpMacro, sizeX, sizeY);

  //BOOST_LOG_TRIVIAL(info)   << "MACRO " << tmpMacro->Name() << "\n"
  //          << "  ORIGIN " << originX << " " << originY << " ;\n"
  //          << "  SIZE   " << sizeX << " " << sizeY << " ;\n";

  return 0;
}

int macroEndCB(lefrCallbackType_e type, const char *macroName, lefiUserData userData) {
  if (type != lefrMacroEndCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not lefrMacroEndCbkType!" << std::endl;
    exit(2);
  }
  Circuit &circuit = *((Circuit *) userData);
  auto &tmpMacro = circuit.getTechRef().last_blk_type_;
  DaliExpects(tmpMacro != nullptr, "A MACRO end before begin?");
  //BOOST_LOG_TRIVIAL(info)   << "END " << tmpMacro->Name() << "\n";
  for (auto &pin: *(tmpMacro->PinList())) {
    pin.InitOffset();
  }

  tmpMacro = nullptr;

  return 0;
}

void ReadLef(std::string const &lef_file_name, Circuit *circuit) {
  DaliExpects(circuit != nullptr, "Cannot read LEF file for a null Circuit instance");
  //lefrInit();
  lefrInitSession(1);
  lefrSetUserData((lefiUserData) circuit);

  lefrSetUnitsCbk(getLefUnits);
  lefrSetManufacturingCbk(getLefManufacturingGrid);
  lefrSetLayerCbk(getLefLayers);
  lefrSetSiteCbk(siteCB);

  lefrSetMacroBeginCbk(macroBeginCB);
  lefrSetMacroCbk(getLefMacros);
  lefrSetPinCbk(getLefPins);
  lefrSetMacroEndCbk(macroEndCB);

  FILE *f;
  if ((f = fopen(lef_file_name.c_str(), "r")) == nullptr) {
    BOOST_LOG_TRIVIAL(info) << "Couldn't open lef file: " << lef_file_name << std::endl;
    exit(2);
  }

  int res = lefrRead(f, lef_file_name.c_str(), (lefiUserData) circuit);
  if (res != 0) {
    BOOST_LOG_TRIVIAL(info) << "LEF parser returns an error!" << std::endl;
    exit(2);
  }
  fclose(f);

  lefrClear();
}

int designCB(defrCallbackType_e type, const char *designName, defiUserData userData) {
  if (type != defrDesignStartCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not defrDesignStartCbkType!" << std::endl;
    exit(2);
  }
  if (!designName || !*designName) {
    DaliExpects(false, "Design name is null, terminate parsing.\n");
  }
  Circuit &circuit = *((Circuit *) userData);
  circuit.getDesignRef().name_ = std::string(designName);
  BOOST_LOG_TRIVIAL(info) << circuit.getDesignRef().name_ << "\n";
  return 0;
}

int getDefUnits(defrCallbackType_e type, double number, defiUserData userData) {
  if (type != defrUnitsCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not defrUnitsCbkType!" << std::endl;
    exit(2);
  }
  Circuit &circuit = *((Circuit *) userData);
  circuit.setUnitsDistanceMicrons(int(number));
  BOOST_LOG_TRIVIAL(trace) << "UNITS DISTANCE MICRONS " << circuit.DistanceMicrons() << " ;" << std::endl;
  return 0;
}

int getDefDieArea(defrCallbackType_e type, defiBox *box, defiUserData userData) {
  if (type != defrDieAreaCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not defrDieAreaCbkType!" << std::endl;
    exit(1);
  }
  Circuit &circuit = *((Circuit *) userData);
  if (!circuit.getDesignRef().die_area_set_) {
    circuit.setDieArea(box->xl(), box->yl(), box->xh(), box->yh());
    BOOST_LOG_TRIVIAL(info) << "Placement region: "
                            << circuit.RegionLLX() << "  "
                            << circuit.RegionLLY() << "  "
                            << circuit.RegionURX() << "  "
                            << circuit.RegionURY() << "\n";
  } else {
    int components_count = circuit.getDesignRef().def_blk_count_;
    int pins_count = circuit.getDesignRef().def_iopin_count_;
    int nets_count = circuit.getDesignRef().def_net_count_;
    circuit.setListCapacity(components_count, pins_count, nets_count);
    BOOST_LOG_TRIVIAL(info) << "components count: " << components_count << "\n"
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
      circuit.getDesignRef().def_blk_count_ = num;
      break;
    }
    case defrStartPinsCbkType : {
      name = "PINS";
      circuit.getDesignRef().def_iopin_count_ = num;
      break;
    }
    case defrNetStartCbkType : {
      name = "NETS";
      circuit.getDesignRef().def_net_count_ = num;
      break;
    }
    default : {
      name = "BOGUS";
      DaliExpects(false, "Unsupported callback types");
    }
  }
  return 0;
}

int getDefTracks(defrCallbackType_e type, defiTrack *track, defiUserData userData) {
  if (type != defrTrackCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not defrTrackCbkType!" << std::endl;
    exit(2);
  }

  return 0;
}

int getDefComponents(defrCallbackType_e type, defiComponent *comp, defiUserData userData) {
  if (type != defrComponentCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not defrComponentCbkType!" << std::endl;
    exit(1);
  }

  Circuit &circuit = *((Circuit *) userData);
  DaliExpects(circuit.getDesignRef().die_area_set_, "Die area not provided, cannot proceed");

  if (!comp->id() || !*comp->id()) {
    DaliExpects(false, "Component name is null, terminate parsing.\n");
  }
  std::string blk_name(comp->id());
  if (!comp->name() || !*comp->name()) {
    DaliExpects(false, "Component macro name is null, terminate parsing.\n");
  }
  std::string blk_type_name(comp->name());
  //BOOST_LOG_TRIVIAL(info)   << "ID: " << comp->id() << " NAME: " << comp->name() << "\n";
  //BOOST_LOG_TRIVIAL(info)   << circuit.GridValueX() << "  " << circuit.GridValueY() << "\n";
  //BOOST_LOG_TRIVIAL(info)   << comp->placementX() << "  " << comp->placementY() << "\n";
  int llx_int =
      (int) std::round(comp->placementX() / circuit.GridValueX() / circuit.DistanceMicrons());
  int lly_int =
      (int) std::round(comp->placementY() / circuit.GridValueY() / circuit.DistanceMicrons());
  //BOOST_LOG_TRIVIAL(info)   << llx_int << "  " << lly_int << "\n";
  std::string orient(std::string(comp->placementOrientStr()));
  PlaceStatus place_status = UNPLACED;
  if (comp->isFixed()) {
    place_status = FIXED;
  } else if (comp->isCover()) {
    place_status = COVER;
  } else if (comp->isPlaced()) {
    place_status = PLACED;
  } else if (comp->isUnplaced()) {
    place_status = UNPLACED;
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
    BOOST_LOG_TRIVIAL(info) << "Type is not defrPinCbkType!" << std::endl;
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
      place_status = FIXED;
      iopin_x =
          (int) std::round(pin->placementX() / circuit.GridValueX() / circuit.DistanceMicrons());
      iopin_y =
          (int) std::round(pin->placementY() / circuit.GridValueY() / circuit.DistanceMicrons());
      is_loc_set = true;
    } else if (pin->isCover()) {
      place_status = COVER;
      iopin_x =
          (int) std::round(pin->placementX() / circuit.GridValueX() / circuit.DistanceMicrons());
      iopin_y =
          (int) std::round(pin->placementY() / circuit.GridValueY() / circuit.DistanceMicrons());
      is_loc_set = true;
    } else if (pin->isPlaced()) {
      place_status = PLACED;
      iopin_x =
          (int) std::round(pin->placementX() / circuit.GridValueX() / circuit.DistanceMicrons());
      iopin_y =
          (int) std::round(pin->placementY() / circuit.GridValueY() / circuit.DistanceMicrons());
      is_loc_set = true;
    } else if (pin->isUnplaced()) {
      place_status = UNPLACED;
    } else {
      place_status = UNPLACED;
    }
  }
  std::string str_sig_use;
  if (pin->hasUse()) {
    str_sig_use = std::string(pin->use());
  }
  DaliExpects(!str_sig_use.empty(), "Please provide IOPIN use");
  SignalUse sig_use = StrToSignalUse(str_sig_use);

  std::string str_sig_dir;
  if (pin->hasDirection()) {
    str_sig_dir = std::string(pin->direction());
  }
  SignalDirection sig_dir = StrToSignalDirection(str_sig_dir);

  if (is_loc_set) {
    circuit.AddIOPin(iopin_name, PLACED, sig_use, sig_dir, iopin_x, iopin_y);
  } else {
    circuit.AddIOPin(iopin_name, UNPLACED, sig_use, sig_dir);
  }

  return 0;

}

int getDefNets(defrCallbackType_e type, defiNet *net, defiUserData userData) {
  if (type != defrNetCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not defrNetCbkType!" << std::endl;
    exit(2);
  }

  Circuit &circuit = *((Circuit *) userData);
  std::string net_name(net->name());
  int net_capacity = int(net->numConnections());
  circuit.AddNet(net_name, net_capacity, circuit.getDesignRef().normal_signal_weight);

  //TO-DO: IOPIN in nets not added
  //BOOST_LOG_TRIVIAL(info)   << "- " << net_name;
  for (int i = 0; i < net->numConnections(); i++) {
    std::string blk_name(net->instance(i));
    std::string pin_name(net->pin(i));
    if (blk_name == "PIN") {
      circuit.AddIOPinToNet(pin_name, net_name);
    } else {
      circuit.AddBlkPinToNet(blk_name, pin_name, net_name);
    }
    //BOOST_LOG_TRIVIAL(info) << " ( " << blk_name << ", " << pin_name << " ) ";
  }
  //BOOST_LOG_TRIVIAL(info) << "\n";

  return 0;
}

int getDefVoid(defrCallbackType_e type, void *dummy, defiUserData userData) {
  if (type != defrDesignEndCbkType) {
    BOOST_LOG_TRIVIAL(info) << "Type is not defrDesignEndCbkType!" << std::endl;
    exit(2);
  }
  BOOST_LOG_TRIVIAL(trace) << "END of DEF\n";
  return 0;
}

void ReadDef(std::string const &def_file_name, Circuit *circuit) {
  DaliExpects(circuit != nullptr, "Cannot read DEF file for a null Circuit instance");

  defrInit();
  defrReset();

  defrInitSession(1);
  defrSetUserData((defiUserData) circuit);
  defrSetDesignCbk(designCB);
  defrSetUnitsCbk(getDefUnits);
  defrSetDieAreaCbk(getDefDieArea);
  defrSetComponentStartCbk(countNumberCB);
  defrSetNetStartCbk(countNumberCB);
  defrSetStartPinsCbk(countNumberCB);

  FILE *f;
  if ((f = fopen(def_file_name.c_str(), "r")) == nullptr) {
    BOOST_LOG_TRIVIAL(info) << "Couldn't open def file" << std::endl;
    exit(2);
  }

  int res = defrRead(f, def_file_name.c_str(), (defiUserData) circuit, 1);
  if (res != 0) {
    BOOST_LOG_TRIVIAL(info) << "DEF parser returns an error in round 1!" << std::endl;
    exit(2);
  }
  fclose(f);

  defrInitSession(2);
  defrSetUserData((defiUserData) circuit);
  //defrSetDesignCbk(designCB);
  //defrSetUnitsCbk(getDefUnits);
  defrSetDieAreaCbk(getDefDieArea);
  defrSetComponentCbk(getDefComponents);

  defrSetPinCbk(getDefIOPins);

  defrSetAddPathToNet();
  defrSetNetCbk(getDefNets);

  //defrSetDesignEndCbk(getDefVoid);
  if ((f = fopen(def_file_name.c_str(), "r")) == nullptr) {
    BOOST_LOG_TRIVIAL(info) << "Couldn't open def file" << std::endl;
    exit(2);
  }
  res = defrRead(f, def_file_name.c_str(), (defiUserData) circuit, 1);
  if (res != 0) {
    BOOST_LOG_TRIVIAL(info) << "DEF parser returns an error in round 2!" << std::endl;
    exit(2);
  }
  fclose(f);

  //numPins = readPinCnt;

  defrClear();
}

}
