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
#ifndef DALI_PLACER_WELL_LEGALIZER_WELL_GEOMETRY_EXPORTER_H_
#define DALI_PLACER_WELL_LEGALIZER_WELL_GEOMETRY_EXPORTER_H_

#include <string>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/well_geometry.h"

namespace dali {

/** Writes canonical well geometry to routing files and PhyDB. */
class WellGeometryExporter {
 public:
  WellGeometryExporter(Circuit* circuit, RectI placement_region,
                       const std::vector<WellGeometryRect>& geometry);

  /** Write N+/P+ rectangles in Dali's routing-rectangle format. */
  void EmitImplantRectFile(const std::string& file_name) const;

  /** Write selected N/P-well rectangles in routing-rectangle format. */
  void EmitWellRectFile(const std::string& file_name, int well_emit_mode) const;

  /** Export N+/P+ rectangles to a PhyDB layout container. */
  void ExportImplantsToPhyDB(phydb::PhyDB* phydb) const;

  /** Export selected N/P-well rectangles to a PhyDB layout container. */
  void ExportWellsToPhyDB(phydb::PhyDB* phydb, int well_emit_mode) const;

 private:
  /** Return true when the selected mode includes this well layer. */
  bool ShouldEmitWellLayer(WellGeometryLayer layer, int well_emit_mode) const;

  /** Convert a Dali-grid rectangle to PhyDB database coordinates. */
  RectI ToPhyDBRect(const RectI& rect) const;

  Circuit* circuit_ = nullptr;
  RectI placement_region_;
  const std::vector<WellGeometryRect>& geometry_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_WELL_GEOMETRY_EXPORTER_H_
