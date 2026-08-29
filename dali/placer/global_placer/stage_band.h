/*******************************************************************************
 *
 * Confine each pipeline stage to a horizontal band, so that a datapath places
 * as stages rather than as a gradient.
 *
 * Fixing a pipeline's inputs to one placement boundary and its outputs to the
 * opposite one orders the stages but does not separate them. Each bit's chain
 * is independent, so it is free to make its progress across the die at its own
 * rate, and nothing asks bit 7's stage-3 cells to share a coordinate with bit
 * 40's. Measured on the 64-bit `bd_pipeline` benchmark, the eight latch banks
 * came out monotonically ordered bottom to top and still unreadable: from stage
 * 3 on, the gap between adjacent band centres was smaller than the bands' own
 * standard deviation, so neighbours interpenetrated. Density spreading makes it
 * worse by pushing cells apart in both axes at once.
 *
 * The missing force is supplied here: every stage gets an interval of the
 * placement region, and its cells are mapped into it.
 *
 * Band heights are proportional to cell area rather than uniform. A pipeline's
 * stages are not the same size -- two of `bd_pipeline`'s eight carry a
 * ~12000 ps adder against ~600 ps elsewhere -- and equal bands would make those
 * stages locally dense while the rest sat empty. Area-proportional heights give
 * every band the same occupancy as the placement as a whole, so introducing
 * bands does not by itself create a density hotspot.
 *
 * This header exposes the arithmetic only. Which cells belong to which stage is
 * the caller's declaration, and the affine mapping of a band's cells into its
 * interval belongs to the placer that owns their coordinates.
 *
 ******************************************************************************/

#ifndef DALI_PLACER_GLOBAL_PLACER_STAGE_BAND_H_
#define DALI_PLACER_GLOBAL_PLACER_STAGE_BAND_H_

#include <vector>

namespace dali {

/** The vertical interval one stage's cells are mapped into. */
struct StageBandInterval {
  double y_lo = 0.0;
  double y_hi = 0.0;

  double Height() const { return y_hi - y_lo; }
};

/** How a region's height is divided among the bands that share it. */
enum class StageBandSpacing {
  /**
   * Height proportional to cell area, so every band carries the placement's
   * own occupancy and introducing bands creates no density hotspot. Bands are
   * then unequally spaced, because stages are unequally sized.
   */
  kAreaProportional,
  /**
   * Equal height for every band.
   *
   * A chain of two-pin nets laid along a line has a degenerate wirelength: the
   * interior terms of sum |x_(i+1) - x_i| cancel and only the endpoints
   * survive, so every spacing costs the same and the objective expresses no
   * preference at all. Interior positions are then decided by density
   * spreading rather than by intent, and stages bunch up. Choosing equal
   * spacing picks one canonical arrangement out of that indifferent set at no
   * wirelength cost. The trade is density: an oversized stage in an
   * equal-height band is locally tighter than the placement as a whole.
   */
  kUniform,
};

/**
 * Divide a region's height among bands in proportion to their cell area.
 *
 * Bands are laid out in the order given, from `region_lo` upward, so the
 * caller's declaration order is the direction the pipeline flows. Every band
 * receives the same area-to-height ratio and therefore the same occupancy as
 * the region overall.
 *
 * A band with no area still receives a zero-height interval at the right place
 * in the stack, so a caller may index the result by stage number without
 * checking whether a stage turned out to be empty. Returns an empty vector when
 * the region has no height or no band has any area, which are the two cases
 * where no meaningful division exists.
 */
std::vector<StageBandInterval> BuildStageBandIntervals(
    const std::vector<double> &areas, double region_lo, double region_hi,
    StageBandSpacing spacing = StageBandSpacing::kAreaProportional);

} // namespace dali

#endif // DALI_PLACER_GLOBAL_PLACER_STAGE_BAND_H_
