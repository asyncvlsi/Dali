#!/usr/bin/env bash
#
# Fail if a source uses a <cmath> macro without including <cmath>.
#
# NAN, INFINITY and HUGE_VAL are macros, not functions, so nothing drags them in
# by accident the way an inline template does. libc++ happens to expose them
# through other standard headers and libstdc++ does not, so a file that relies
# on that compiles on macOS and fails on the CI Linux image with
# "'NAN' was not declared in this scope" -- which is exactly how CI failed after
# the no-Galois link errors were cleared.
#
# This is a text check on purpose. The condition is a missing include, not a
# missing symbol, so no amount of building on this machine can find it; the only
# local signal available is the source itself.
set -u
cd "${1:-.}"

status=0
while IFS= read -r file; do
  case "$file" in
    */scripts/check_math_macro_includes.sh) continue ;;
  esac
  grep -qE '(^|[^A-Za-z_])(NAN|INFINITY|HUGE_VAL)([^A-Za-z_]|$)' "$file" || continue
  grep -qE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"](cmath|math\.h)[>"]' "$file" && continue
  if [ "$status" -eq 0 ]; then
    echo "sources using a <cmath> macro without including <cmath>:" >&2
  fi
  echo "  $file: $(grep -oE '(NAN|INFINITY|HUGE_VAL)' "$file" | sort -u | tr '\n' ' ')" >&2
  status=1
done < <(git ls-files '*.cc' '*.h' '*.hpp' '*.cpp')

if [ "$status" -ne 0 ]; then
  echo >&2
  echo "These compile against libc++ and fail against libstdc++. Add" >&2
  echo "#include <cmath> to each." >&2
  exit 1
fi
echo "cmath macro includes: OK"
