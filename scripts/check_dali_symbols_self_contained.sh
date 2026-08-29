#!/usr/bin/env bash
#
# Fail if anything in a build tree references a dali:: symbol nothing defines.
#
# Compiling is not linking. A method declared unconditionally in dali.h and
# defined only inside `#if PHYDB_USE_GALOIS` compiles into every object that
# calls it and then fails at link time. CI has now hit that twice: once linking
# bin/dali, and once linking a test binary that called two placement helpers
# which had no business being behind the timing-host guard at all.
#
# The whole tree is scanned, not just libdalilib.a, because the second failure
# was referenced from a test object and the library alone looked self-contained.
#
# Linking locally is not an option when the installed PhyDB is Galois-enabled:
# that PhyDB pulls in galois::eda symbols a forced no-Galois link cannot
# resolve, an artefact of the mixture that never occurs on CI. Symbol closure
# over dali:: is the part that must hold and is checkable anywhere.
#
# With two arguments the first is a cmake to build the tree with first. Link
# errors during that build are expected locally and ignored: the objects are
# what this needs, and they are produced regardless. Doing the build here rather
# than as a separate CMake COMMAND keeps the "ignore link failures" part out of
# CMake's argument quoting, which mangled it into /bin/sh.
set -u
if [ "$#" -ge 2 ]; then
  cmake_bin="$1"
  root="$2"
  build_log="$(mktemp)"
  "$cmake_bin" --build "$root" -- -k > "$build_log" 2>&1 || true
  # Link failures are expected locally and are exactly what the symbol closure
  # below replaces. Compile failures are not: they leave stale objects behind,
  # and scanning those reports a cheerful OK for a tree that did not build.
  compile_errors="$(grep -E "error:" "$build_log" \
                    | grep -vE "linker command failed|^ld:|ld: symbol" || true)"
  if [ -n "$compile_errors" ]; then
    echo "no-Galois build has compile errors, so its objects are stale:" >&2
    printf '%s\n' "$compile_errors" | head -20 >&2
    rm -f "$build_log"
    exit 2
  fi
  rm -f "$build_log"
else
  root="${1:?usage: check_dali_symbols_self_contained.sh [<cmake>] <build-dir-or-archive>}"
fi
[ -e "$root" ] || { echo "missing path: $root" >&2; exit 2; }

# No `mapfile`: macOS still ships bash 3.2, and a gate that only runs on the
# CI image is not a local gate.
list_file="$(mktemp)"
trap 'rm -f "$list_file"' EXIT
if [ -d "$root" ]; then
  find "$root" \( -name '*.o' -o -name '*.a' \) | sort > "$list_file"
else
  printf '%s\n' "$root" > "$list_file"
fi
object_count="$(wc -l < "$list_file" | tr -d ' ')"
[ "$object_count" -gt 0 ] || { echo "no objects under $root" >&2; exit 2; }

nm_out="$(tr '\n' '\0' < "$list_file" | xargs -0 nm 2>/dev/null)" \
  || { echo "nm failed under $root" >&2; exit 2; }

defined="$(printf '%s\n' "$nm_out" | awk '$2 ~ /^[TtWwSsDdBbVvRr]$/ {print $3}' | sort -u)"
undefined="$(printf '%s\n' "$nm_out" | awk '$1 == "U" {print $2} $2 == "U" {print $3}' | sort -u)"

missing="$(comm -23 <(printf '%s\n' "$undefined") <(printf '%s\n' "$defined") \
           | grep -E '^_?_ZN4dali' || true)"

if [ -n "$missing" ]; then
  echo "dali symbols referenced but defined nowhere under $root:" >&2
  printf '%s\n' "$missing" | while read -r symbol; do
    [ -z "$symbol" ] && continue
    if command -v c++filt > /dev/null 2>&1; then
      echo "  $(c++filt "$symbol")" >&2
    else
      echo "  $symbol" >&2
    fi
  done
  echo >&2
  echo "Each is declared unconditionally but defined only under a build" >&2
  echo "condition this configuration does not meet. Either give it a fallback" >&2
  echo "beside the other no-Galois definitions in dali/dali.cc, or move it out" >&2
  echo "of the guard if it never needed a timing host." >&2
  exit 1
fi
echo "dali symbol closure: OK ($object_count objects under $root)"
