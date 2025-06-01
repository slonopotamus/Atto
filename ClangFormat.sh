#!/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

PROJECT_DIR="$(dirname "${BASH_SOURCE[0]}")"

if [[ "${OSTYPE}" == "darwin"* ]]; then
  function nproc {
    sysctl -n hw.logicalcpu
  }
fi

find "${PROJECT_DIR}" \( -name "*.h" -o -name "*.cpp" \) -print0 | xargs -0 -P "$(nproc)" -n 100 clang-format -i
