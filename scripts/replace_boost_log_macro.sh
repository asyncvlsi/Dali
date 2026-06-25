#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v rg >/dev/null 2>&1; then
  echo "ripgrep (rg) is required" >&2
  exit 1
fi

tmp_file="$(mktemp)"
trap 'rm -f "${tmp_file}"' EXIT

rg -0 -l 'BOOST_LOG_TRIVIAL' "${repo_root}/dali" "${repo_root}/tests" >"${tmp_file}" || true

if [[ ! -s "${tmp_file}" ]]; then
  echo "No BOOST_LOG_TRIVIAL call sites found."
  exit 0
fi

xargs -0 perl -pi -e 's/\bBOOST_LOG_TRIVIAL\s*\(/LOG(/g' <"${tmp_file}"
echo "Updated $(tr '\0' '\n' <"${tmp_file}" | wc -l | tr -d ' ') file(s)."
