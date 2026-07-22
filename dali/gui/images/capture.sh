#!/usr/bin/env bash
#
# Regenerate the screenshots in this directory.
#
# Usage: ./capture.sh <output-dir> <dali command...>
#
# Pass -enable_end_cap_cell in the command for the end-cap image to show them.
#
# Runs the given placement command with the GUI's capture hook enabled, so the
# images can be reproduced from any design rather than a particular one. The
# regions are fractions of the design bounding box, so they frame comparable
# areas whatever the design size. Rendering is offscreen: no display needed.
set -euo pipefail

if [ "$#" -lt 2 ]; then
  echo "usage: $0 <output-dir> <dali command...>" >&2
  exit 1
fi

out_dir="$1"
shift
mkdir -p "$out_dir"

export QT_QPA_PLATFORM=offscreen
export DALI_GUI_CAPTURE_SIZE="1100x820"
export DALI_GUI_CAPTURE="$out_dir\
;01_global_placement@global_placement.final:fit\
;02_legalized@final:fit\
;03_stripe_columns@final:0.05,0.35,0.45,0.72+cells\
;04_cells_and_taps@final:0.10,0.45,0.20,0.55+cells\
;05_end_caps@final:0.09,0.46,0.13,0.51+cells"

"$@" -gui_debug -gui_pause off &
dali_pid=$!

# The GUI window stays open after placement finishes, so wait for the last
# image instead of for the process to exit.
for _ in $(seq 1 120); do
  sleep 5
  [ -f "$out_dir/05_end_caps.png" ] && break
done
sleep 2
kill "$dali_pid" 2>/dev/null || true
ls -l "$out_dir"
