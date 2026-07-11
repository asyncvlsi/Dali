/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/
#include "dali/placer/well_legalizer/well_geometry_exporter.h"

#include <fstream>

namespace dali {

WellGeometryExporter::WellGeometryExporter(
    Circuit* circuit, RectI placement_region,
    const std::vector<WellGeometryRect>& geometry)
    : circuit_(circuit),
      placement_region_(placement_region),
      geometry_(geometry) {
  DaliExpects(circuit_ != nullptr, "Well geometry export requires a circuit");
}

bool WellGeometryExporter::ShouldEmitWellLayer(WellGeometryLayer layer,
                                               int well_emit_mode) const {
  DaliExpects(well_emit_mode >= 0 && well_emit_mode <= 2,
              "Invalid well emission mode");
  if (layer == WellGeometryLayer::kPwell) {
    return well_emit_mode != 1;
  }
  if (layer == WellGeometryLayer::kNwell) {
    return well_emit_mode != 2;
  }
  return false;
}

RectI WellGeometryExporter::ToPhyDBRect(const RectI& rect) const {
  return RectI(circuit_->LocDali2PhydbX(rect.LLX()),
               circuit_->LocDali2PhydbY(rect.LLY()),
               circuit_->LocDali2PhydbX(rect.URX()),
               circuit_->LocDali2PhydbY(rect.URY()));
}

void WellGeometryExporter::EmitImplantRectFile(
    const std::string& file_name) const {
  LOG(info) << "Writing PP and NP rect file: " << file_name << "\n";
  std::ofstream output(file_name);
  DaliExpects(output.is_open(), "Cannot open output file: " + file_name);

  RectI bbox = ToPhyDBRect(placement_region_);
  output << "bbox " << bbox.LLX() << " " << bbox.LLY() << " " << bbox.URX()
         << " " << bbox.URY() << "\n";
  for (const WellGeometryRect& geometry_rect : geometry_) {
    std::string layer_name;
    if (geometry_rect.layer == WellGeometryLayer::kNplus) {
      layer_name = "nplus";
    } else if (geometry_rect.layer == WellGeometryLayer::kPplus) {
      layer_name = "pplus";
    } else {
      continue;
    }
    RectI rect = ToPhyDBRect(geometry_rect.bounds);
    output << "rect # " << layer_name << " " << rect.LLX() << "\t" << rect.LLY()
           << "\t" << rect.URX() << "\t" << rect.URY() << "\n";
  }
}

void WellGeometryExporter::EmitWellRectFile(const std::string& file_name,
                                            int well_emit_mode) const {
  DaliExpects(well_emit_mode >= 0 && well_emit_mode <= 2,
              "Invalid well emission mode");
  if (well_emit_mode == 0) {
    LOG(info) << "Writing N/P-well rect file: " << file_name << "\n";
  } else if (well_emit_mode == 1) {
    LOG(info) << "Writing N-well rect file: " << file_name << "\n";
  } else {
    LOG(info) << "Writing P-well rect file: " << file_name << "\n";
  }

  std::ofstream output(file_name);
  DaliExpects(output.is_open(), "Cannot open output file: " + file_name);
  RectI bbox = ToPhyDBRect(placement_region_);
  output << "bbox " << bbox.LLX() << " " << bbox.LLY() << " " << bbox.URX()
         << " " << bbox.URY() << "\n";
  for (const WellGeometryRect& geometry_rect : geometry_) {
    if (!ShouldEmitWellLayer(geometry_rect.layer, well_emit_mode)) {
      continue;
    }
    const char* signal_name =
        geometry_rect.layer == WellGeometryLayer::kPwell ? "GND" : "Vdd";
    const char* layer_name =
        geometry_rect.layer == WellGeometryLayer::kPwell ? "pwell" : "nwell";
    RectI rect = ToPhyDBRect(geometry_rect.bounds);
    output << "rect " << signal_name << " " << layer_name << " " << rect.LLX()
           << " " << rect.LLY() << " " << rect.URX() << " " << rect.URY()
           << "\n";
  }
}

void WellGeometryExporter::ExportImplantsToPhyDB(phydb::PhyDB* phydb) const {
  DaliExpects(phydb != nullptr, "Cannot export implants to a null PhyDB");
  LOG(info) << "Export Pplus/Nplus fillings to PhyDB\n";
  RectI bbox = ToPhyDBRect(placement_region_);
  auto* layout = phydb->CreatePpNpMacroAndComponent(bbox.LLX(), bbox.LLY(),
                                                    bbox.URX(), bbox.URY());

  for (const WellGeometryRect& geometry_rect : geometry_) {
    std::string layer_name;
    if (geometry_rect.layer == WellGeometryLayer::kNplus) {
      layer_name = "nplus";
    } else if (geometry_rect.layer == WellGeometryLayer::kPplus) {
      layer_name = "pplus";
    } else {
      continue;
    }
    RectI rect = ToPhyDBRect(geometry_rect.bounds);
    std::string signal_name = "#";
    layout->AddRectSignalLayer(signal_name, layer_name, rect.LLX(), rect.LLY(),
                               rect.URX(), rect.URY());
  }
}

void WellGeometryExporter::ExportWellsToPhyDB(phydb::PhyDB* phydb,
                                              int well_emit_mode) const {
  DaliExpects(phydb != nullptr, "Cannot export wells to a null PhyDB");
  DaliExpects(well_emit_mode >= 0 && well_emit_mode <= 2,
              "Invalid well emission mode");
  if (well_emit_mode == 0) {
    LOG(info) << "Export N/P wells to PhyDB\n";
  } else if (well_emit_mode == 1) {
    LOG(info) << "Export N wells to PhyDB\n";
  } else {
    LOG(info) << "Export P wells to PhyDB\n";
  }

  RectI bbox = ToPhyDBRect(placement_region_);
  auto* layout = phydb->CreateWellLayerMacroAndComponent(
      bbox.LLX(), bbox.LLY(), bbox.URX(), bbox.URY());
  for (const WellGeometryRect& geometry_rect : geometry_) {
    if (!ShouldEmitWellLayer(geometry_rect.layer, well_emit_mode)) {
      continue;
    }
    std::string signal_name =
        geometry_rect.layer == WellGeometryLayer::kPwell ? "GND" : "Vdd";
    std::string layer_name =
        geometry_rect.layer == WellGeometryLayer::kPwell ? "pwell" : "nwell";
    RectI rect = ToPhyDBRect(geometry_rect.bounds);
    layout->AddRectSignalLayer(signal_name, layer_name, rect.LLX(), rect.LLY(),
                               rect.URX(), rect.URY());
  }
}

}  // namespace dali
