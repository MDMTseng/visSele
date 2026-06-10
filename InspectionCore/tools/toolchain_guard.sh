#!/usr/bin/env bash
# Toolchain-drift guard.
#
# A vcpkg (re)configure rebuilds every dependency from source (~30 min) when the
# toolchain vcpkg folds into its package ABIs changes. The two things that move
# "on their own": C:\vcpkg getting re-cloned/updated (new bundled cmake/
# powershell + helper ports) and MSYS2 bumping gcc. This guard checks both
# against the pinned fingerprint and ABORTS the configure on drift, so a 30-min
# rebuild can never be triggered by accident.
#
# Pins:
#   * vcpkg commit  == "builtin-baseline" in ../vcpkg.json
#   * gcc version   == GCC_VERSION in ./toolchain.lock
#
# Override (when you actually mean to move the toolchain): ALLOW_REBUILD=1.
# Reads $VCPKG_ROOT for the vcpkg checkout (default: $HOME/vcpkg).

set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"

drift=0
msg=""

# --- vcpkg checkout vs builtin-baseline ----------------------------------
baseline=""
if [[ -f "$ROOT/vcpkg.json" ]]; then
  baseline=$(grep -oE '"builtin-baseline"[[:space:]]*:[[:space:]]*"[0-9a-f]{40}"' "$ROOT/vcpkg.json" \
             | grep -oE '[0-9a-f]{40}' || true)
fi
if [[ -n "$baseline" && -d "$VCPKG_ROOT/.git" ]]; then
  head=$(git -C "$VCPKG_ROOT" rev-parse HEAD 2>/dev/null || echo "")
  if [[ -n "$head" && "$head" != "$baseline" ]]; then
    drift=1
    msg+=$'\n'"    vcpkg moved : HEAD ${head:0:12} != baseline ${baseline:0:12}"
  fi
fi

# --- gcc version vs lock -------------------------------------------------
if [[ -f "$DIR/toolchain.lock" ]]; then
  want=$(grep -oE '^GCC_VERSION=[0-9.]+' "$DIR/toolchain.lock" | cut -d= -f2 || true)
  have=$(gcc -dumpfullversion 2>/dev/null || echo "")
  if [[ -n "$want" && -n "$have" && "$want" != "$have" ]]; then
    drift=1
    msg+=$'\n'"    gcc changed : $have != locked $want"
  fi
fi

if (( drift )); then
  {
    echo "=================================================================="
    echo " TOOLCHAIN DRIFT -- a (re)configure now WILL rebuild all vcpkg"
    echo " dependencies from source (~30 min). Cause(s):$msg"
    echo ""
    echo " To AVOID the rebuild: point \$VCPKG_ROOT back at commit"
    echo "   ${baseline:0:12}  (git -C \"\$VCPKG_ROOT\" checkout $baseline)"
    echo " and keep MSYS2/MinGW gcc at the locked version (see tools/toolchain.lock)."
    echo ""
    echo " If you DELIBERATELY want to move the toolchain, re-run with"
    echo "   ALLOW_REBUILD=1   (build.sh: --allow-rebuild)"
    echo " then update tools/toolchain.lock + vcpkg.json builtin-baseline."
    echo "=================================================================="
  } >&2
  if [[ -z "${ALLOW_REBUILD:-}" ]]; then
    exit 3
  fi
  echo "ALLOW_REBUILD set -> proceeding despite toolchain drift." >&2
fi
exit 0
