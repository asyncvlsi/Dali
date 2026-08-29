/*******************************************************************************
 *
 * Geometry and chain discovery for delay-line detours. See the header for why
 * endpoints are pinned and why no slack-to-amplitude conversion lives here.
 *
 ******************************************************************************/

#include "dali/timing/delay_line_detour.h"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "dali/circuit/circuit.h"

namespace dali {

namespace {

/** Distance below which two endpoints are treated as the same point. */
constexpr double kDegenerateAxis = 1e-9;

/** Return the net a component drives, or -1 when it drives none. */
int FindDriverNet(
    const std::unordered_map<int, std::vector<std::pair<int, bool>>>
        &nets_by_component,
    int component_id) {
  auto found = nets_by_component.find(component_id);
  if (found == nets_by_component.end()) return -1;
  for (const auto &[net_id, is_input] : found->second) {
    if (!is_input) return net_id;
  }
  return -1;
}

} // namespace

std::vector<DetourTarget> BuildZigzagDetour(const DelayLineChain &chain,
                                            double amplitude) {
  std::vector<DetourTarget> targets;
  int count = static_cast<int>(chain.nodes.size());
  if (count < 3 || amplitude <= 0.0) return targets;

  const DelayLineNode &head = chain.nodes.front();
  const DelayLineNode &tail = chain.nodes.back();
  double axis_x = tail.x - head.x;
  double axis_y = tail.y - head.y;
  double length = std::sqrt(axis_x * axis_x + axis_y * axis_y);

  double normal_x = 0.0;
  double normal_y = 1.0;
  if (length > kDegenerateAxis) {
    normal_x = -axis_y / length;
    normal_y = axis_x / length;
  }

  targets.reserve(count - 2);
  for (int index = 1; index < count - 1; ++index) {
    double fraction = static_cast<double>(index) / (count - 1);
    double sign = (index % 2 == 0) ? -1.0 : 1.0;
    targets.push_back({chain.nodes[index].component_id,
                       head.x + axis_x * fraction + normal_x * amplitude * sign,
                       head.y + axis_y * fraction + normal_y * amplitude * sign});
  }
  return targets;
}

std::vector<int> BuildTwoBandRowAssignment(int element_count, int separation) {
  std::vector<int> rows;
  if (element_count <= 0) return rows;
  if (separation < 0) separation = 0;

  rows.reserve(element_count);
  for (int index = 0; index < element_count; ++index) {
    rows.push_back((index % 2 == 1) ? separation : 0);
  }
  return rows;
}

std::vector<DetourTarget> BuildRowBandTargets(const DelayLineChain &chain,
                                              const std::vector<int> &rows,
                                              double row_height,
                                              double column_width) {
  std::vector<DetourTarget> targets;
  if (chain.nodes.empty() || rows.size() != chain.nodes.size()) return targets;

  double base_x = chain.nodes.front().x;
  double base_y = chain.nodes.front().y;
  targets.reserve(chain.nodes.size());
  for (size_t index = 0; index < chain.nodes.size(); ++index) {
    targets.push_back({chain.nodes[index].component_id,
                       base_x + static_cast<double>(index) * column_width,
                       base_y + rows[index] * row_height});
  }
  return targets;
}

int NearestCoprimeStride(int requested, int columns) {
  if (columns <= 1) return 1;
  int stride = std::min(std::max(requested, 1), columns - 1);
  while (stride > 1 && std::gcd(stride, columns) != 1) --stride;
  return stride;
}

int MaxHopStride(int columns) {
  if (columns <= 2) return 1;
  // Evaluated rather than derived. The travel a stride buys is not monotone in
  // the stride: columns are laid out on a line, so a step of `stride` costs
  // that much width, but the map wraps and a wrapped step costs
  // `columns - stride` instead. A large stride therefore makes one long hop and
  // then short ones. With at most a few hundred columns the exact total is
  // cheaper to measure than to reason about, and measuring cannot be wrong.
  int best_stride = 1;
  long long best_travel = -1;
  for (int stride = 1; stride < columns; ++stride) {
    if (std::gcd(stride, columns) != 1) continue;
    long long travel = 0;
    int position = 0;
    for (int step = 1; step < columns; ++step) {
      const int next = (position + stride) % columns;
      travel += std::abs(next - position);
      position = next;
    }
    if (travel > best_travel) {
      best_travel = travel;
      best_stride = stride;
    }
  }
  return best_stride;
}

int EffectiveColumnStride(int columns, int requested) {
  return requested <= 0 ? MaxHopStride(columns)
                        : NearestCoprimeStride(requested, columns);
}

std::vector<DetourTarget> BuildInterleavedRowBandTargets(
    const DelayLineChain &chain, int separation, double row_height,
    double column_width, int column_stride) {
  std::vector<DetourTarget> targets;
  if (chain.nodes.empty() || row_height <= 0.0 || column_width <= 0.0) {
    return targets;
  }
  // A negative separation lays the band below the head instead of above it.
  // The head stays where the placer put it either way; only the direction the
  // band extends changes, which is what lets a chain near the top of the region
  // still reach a large separation.
  //
  // The two rows must stay distinct. Folding puts elements `i` and
  // `count-1-i` in the same column, so a separation of zero would place them
  // on top of each other; one adjacent row is the compact baseline here, not
  // zero.
  if (separation == 0) separation = 1;
  const int count = static_cast<int>(chain.nodes.size());
  const int columns = (count + 1) / 2;
  const int stride = EffectiveColumnStride(columns, column_stride);
  double base_x = chain.nodes.front().x;
  double base_y = chain.nodes.front().y;
  targets.reserve(chain.nodes.size());
  for (int index = 0; index < count; ++index) {
    // The fold pairs index with count-1-index onto one position; permuting the
    // position keeps that pairing, so the two remain in one column and, having
    // opposite parity, in different rows.
    const int fold_position = std::min(index, count - 1 - index);
    const int column = (fold_position * stride) % columns;
    const int row = (index % 2 == 1) ? separation : 0;
    targets.push_back({chain.nodes[index].component_id,
                       base_x + column * column_width,
                       base_y + row * row_height});
  }
  return targets;
}

bool BuildDelayLineChain(Circuit &circuit, const std::string &name_prefix,
                         DelayLineChain *chain, std::string *error_message) {
  if (name_prefix.empty()) {
    *error_message = "delay-line name prefix is empty";
    return false;
  }

  std::vector<Component> &components = circuit.Components();
  std::unordered_set<int> members;
  for (Component &component : components) {
    if (component.Name().compare(0, name_prefix.size(), name_prefix) == 0)
      members.insert(component.Id());
  }
  if (members.empty()) {
    *error_message = "no component name starts with '" + name_prefix + "'";
    return false;
  }

  std::unordered_map<int, std::vector<std::pair<int, bool>>> nets_by_component;
  for (Net &net : circuit.Nets()) {
    for (NetPin &net_pin : net.ComponentPins()) {
      int component_id = net_pin.ComponentId();
      if (members.find(component_id) == members.end()) continue;
      nets_by_component[component_id].emplace_back(net.Id(),
                                                   net_pin.PinPtr()->IsInput());
    }
  }

  std::unordered_map<int, int> successor;
  std::unordered_set<int> has_predecessor;
  for (int component_id : members) {
    int driver_net = FindDriverNet(nets_by_component, component_id);
    if (driver_net < 0) {
      *error_message = "'" + components[component_id].Name() +
                       "' drives no net, so the chain order is undefined";
      return false;
    }
    for (NetPin &net_pin : circuit.Nets()[driver_net].ComponentPins()) {
      int sink_id = net_pin.ComponentId();
      if (sink_id == component_id) continue;
      if (members.find(sink_id) == members.end()) continue;
      if (!net_pin.PinPtr()->IsInput()) continue;
      if (successor.count(component_id) > 0) {
        *error_message = "'" + components[component_id].Name() +
                         "' drives more than one delay-line element, so '" +
                         name_prefix + "' is not a simple chain";
        return false;
      }
      successor[component_id] = sink_id;
      if (!has_predecessor.insert(sink_id).second) {
        *error_message = "'" + components[sink_id].Name() +
                         "' has more than one driver inside '" + name_prefix +
                         "', so it is not a simple chain";
        return false;
      }
    }
  }

  std::vector<int> heads;
  for (int component_id : members) {
    if (has_predecessor.find(component_id) == has_predecessor.end())
      heads.push_back(component_id);
  }
  if (heads.size() != 1) {
    *error_message = "'" + name_prefix + "' has " +
                     std::to_string(heads.size()) +
                     " chain heads, expected exactly 1";
    return false;
  }

  DelayLineChain built;
  built.name = name_prefix;
  int current = heads.front();
  while (true) {
    Component &component = components[current];
    built.nodes.push_back({current, component.LLX(), component.LLY()});
    auto next = successor.find(current);
    if (next == successor.end()) break;
    current = next->second;
  }
  if (built.nodes.size() != members.size()) {
    *error_message = "'" + name_prefix + "' walks " +
                     std::to_string(built.nodes.size()) + " of " +
                     std::to_string(members.size()) +
                     " matched components, so it is not a single chain";
    return false;
  }

  *chain = std::move(built);
  return true;
}

} // namespace dali
