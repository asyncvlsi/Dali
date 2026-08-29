/*******************************************************************************
 *
 * Lengthen a delay line in place, so that placement can repair a relative-
 * timing violation without inserting cells.
 *
 * A bundled-data delay line must cover its stage's logic. The usual repair
 * inserts elements until it does. The other lever is wire: a chain of identical
 * inverters gets abutted by any wirelength-minimizing placer, so its internal
 * nets carry almost no delay, and detouring the chain adds latency at no cell
 * cost. On the `wire_repair` benchmark a delay line built at 53.6% of its fast
 * path violates by -461.0 ps and closes at roughly 66 um of detour amplitude.
 *
 * Two properties of that lever shape this interface.
 *
 * The endpoints must not move. A delay line's last element drives the fork root
 * of the next stage's constraint, so moving it changes when that constraint
 * starts measuring and its slack shifts for a reason unrelated to this delay
 * line. Pinning both ends keeps a detour local, which is what makes detours on
 * different delay lines independent of each other.
 *
 * The delay a detour buys cannot be predicted in closed form. Measured per
 * micron of added wire it varies more than twelvefold across designs under one
 * extraction model, because capacitive loading grows linearly with wire length
 * while RC grows with its square. This header therefore exposes geometry only:
 * callers choose an amplitude, apply it, and measure. Nothing here converts a
 * slack deficit into an amplitude.
 *
 ******************************************************************************/

#ifndef DALI_TIMING_DELAY_LINE_DETOUR_H_
#define DALI_TIMING_DELAY_LINE_DETOUR_H_

#include <string>
#include <vector>

namespace dali {

class Circuit;

/** One element of a delay line, with the location it currently occupies. */
struct DelayLineNode {
  int component_id = -1;
  double x = 0.0;
  double y = 0.0;
};

/** A delay line's elements ordered from chain input to chain output. */
struct DelayLineChain {
  std::string name;
  std::vector<DelayLineNode> nodes;
};

/** A location a detour requests for one component. */
struct DetourTarget {
  int component_id = -1;
  double x = 0.0;
  double y = 0.0;
};

/**
 * Return locations that zigzag a chain's interior, both endpoints pinned.
 *
 * Node locations and `amplitude` must share one isotropic unit, which means
 * microns rather than Dali's internal placement grid: the grid spacings differ
 * between x and y, so a direction perpendicular in grid coordinates is not
 * perpendicular on the die. Callers holding grid coordinates convert on the way
 * in and back on the way out.
 *
 * Interior elements are spread evenly along the segment joining the two
 * endpoints and displaced alternately to either side of it by `amplitude`, so
 * each internal net grows by roughly twice that value. Endpoints are never
 * returned, because a caller that moved them would perturb a neighbouring
 * constraint.
 *
 * Returns nothing when there is no interior to move, when `amplitude` is not
 * positive, or when the chain is too short to have a direction. A chain whose
 * endpoints coincide has no axis to be perpendicular to, so the displacement
 * runs along y, which keeps the result well defined instead of dividing by a
 * vanishing length.
 */
std::vector<DetourTarget> BuildZigzagDetour(const DelayLineChain &chain,
                                            double amplitude);

/**
 * Assign chain elements to rows so consecutive elements sit far apart.
 *
 * The form that suits a gridded flow. Rows here are not declared in the DEF and
 * are not on a fixed pitch: the legalizer builds them bottom-up and each row is
 * exactly tall enough for the tallest cell it holds. A delay line is one macro
 * repeated, so wherever its elements land they define rows of a single known
 * height, and the lattice is seeded rather than derived.
 *
 * That makes the lever a permutation rather than a displacement. The chain runs
 * along the rows, the same direction the datapath it serves runs: a 64-bit
 * stage places its 64 cells across one gridded row, so a delay line covering
 * that stage belongs beside it rather than standing perpendicular to it. A
 * separation of zero leaves the whole chain in one row, abutted, which is the
 * compact baseline. Raising it lifts every other element to a row `separation`
 * away, so the chain zigzags between two rows while still advancing along them,
 * and each hop crosses that many rows.
 *
 * Every value is equally legal: no element leaves a row and no row changes
 * height, so nothing here can be undone by legalization. Alternate elements
 * occupy alternate column slots within their row, so the two rows interleave
 * without overlapping.
 *
 * Consecutive elements need not occupy adjacent rows and the rows they occupy
 * need not be contiguous; intervening rows belong to whatever else the placer
 * puts there. Reachable delay is therefore bounded by available vertical
 * extent, not by how many elements the chain has -- eight elements spanning a
 * far enough separation carry as much added wire as a long chain does.
 *
 * Returns one row index per element, in chain order. Endpoints are included
 * because the caller decides whether to honour them: pinning them keeps a
 * detour from disturbing a neighbouring constraint, which matters when the
 * chain's last element drives another fork's root.
 */
std::vector<int> BuildTwoBandRowAssignment(int element_count, int separation);

/**
 * Place chain elements on the rows a row assignment gives them.
 *
 * Rows are `row_height` apart, measured up from the chain's first element,
 * which is what a delay line's own elements establish: they are one macro
 * repeated, so every row they occupy is exactly that macro tall. Elements
 * advance one `column_width` each along the row, so the chain lies horizontally
 * and the row assignment only decides how far each element is lifted.
 *
 * Locations are in the same unit as `row_height` and `column_width`.
 */
std::vector<DetourTarget> BuildRowBandTargets(const DelayLineChain &chain,
                                              const std::vector<int> &rows,
                                              double row_height,
                                              double column_width);

/**
 * Place a chain folded back on itself inside two rows.
 *
 * The chain runs out along the two rows and returns along them, the return
 * path interleaving into the columns the outbound path left empty:
 *
 *     col:      0    1    2    3    4    5
 *     top:      1   11    3    9    5    7
 *     bottom:  12    2   10    4    8    6
 *
 * `column = min(index, count - 1 - index)` is the whole rule. The fold costs
 * nothing -- no extra rows, and every hop is still one column plus the
 * separation, so latency per element is unchanged. What it buys is that the
 * chain's output returns to the side its input entered from, which is what lets
 * the control circuitry sit on one side of the datapath instead of straddling
 * it, and it halves the width: `count` elements need `count/2` columns.
 *
 * Folding is therefore geometry, and unconditional. It is not a knob and adds
 * no delay.
 *
 * `separation` and `column_stride` are the two timing knobs, and they buy
 * length along different axes. Separation lifts alternate elements, so every
 * hop crosses that many rows. Stride permutes which physical column each fold
 * position occupies -- position `p` moves to `(p * stride) mod columns` -- so
 * consecutive elements sit `stride` columns apart instead of one, and every hop
 * also crosses that much width. The chain is unchanged; only the order in which
 * it visits the columns is.
 *
 * A stride is usable only when it is coprime with the column count, since
 * otherwise the map is not a bijection and elements would collide; a requested
 * stride is reduced to the nearest usable one. Stride 1 is the unpermuted
 * layout, and a stride of zero or less asks for the widest hop the column count
 * allows.
 *
 * Note that the hop a stride produces is `min(stride, columns - stride)`
 * columns, because the map wraps: a stride of `columns - 1` steps backwards by
 * one and buys nothing. The widest hop is therefore near half the column count,
 * which is what the automatic choice returns.
 *
 * Which knob is worth more depends on the chain. Stride's budget is bounded by
 * the column count, so it is weak for a short delay line and strong for a long
 * one, while separation's is bounded by the region height and is the same for
 * both. Neither rate is predictable in closed form -- see this header's opening
 * note -- so callers choose and measure.
 */
std::vector<DetourTarget> BuildInterleavedRowBandTargets(
    const DelayLineChain &chain, int separation, double row_height,
    double column_width, int column_stride = 1);

/**
 * The largest stride at or below `requested` that permutes `columns` columns.
 *
 * A stride maps fold position `p` to `(p * stride) mod columns`, which visits
 * every column exactly once precisely when the two are coprime. Reducing to the
 * nearest coprime keeps a requested stride usable instead of rejecting it, and
 * the search terminates because 1 is coprime with everything.
 */
int NearestCoprimeStride(int requested, int columns);

/**
 * The usable stride producing the widest hop across `columns` columns.
 *
 * Because the column map wraps, hop distance is `min(stride, columns - stride)`
 * and is maximized near half the column count rather than at the largest
 * stride. Searches outward from that midpoint for a coprime value, which always
 * terminates since 1 qualifies.
 */
int MaxHopStride(int columns);

/**
 * Resolve a requested stride against a column count.
 *
 * The one place the "zero means widest, anything else is reduced to a usable
 * value" rule lives, so that callers reporting the stride and the code laying
 * out the columns cannot disagree about which stride was used.
 */
int EffectiveColumnStride(int columns, int requested);

/**
 * Order the components whose names start with `name_prefix` into a chain.
 *
 * Order comes from connectivity rather than from names or from the order the
 * components were read in: an element precedes another when its output net is
 * the other's input. Instance names encode position only by convention, and a
 * detour applied in the wrong order silently produces a shape that lengthens
 * different nets than intended.
 *
 * Fails, leaving `chain` untouched, when the prefix matches nothing, when the
 * matched components do not form a single simple chain, or when an element has
 * no identifiable output. Those all mean the caller named something that is not
 * a delay line, which is worth reporting rather than approximating.
 */
bool BuildDelayLineChain(Circuit &circuit, const std::string &name_prefix,
                         DelayLineChain *chain, std::string *error_message);

} // namespace dali

#endif // DALI_TIMING_DELAY_LINE_DETOUR_H_
