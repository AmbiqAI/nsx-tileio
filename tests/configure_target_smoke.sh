#!/usr/bin/env bash
set -euo pipefail

consumer_root=${1:?consumer root}
toolchain=${2:?toolchain}
board=${3:?board}
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_root/build/target-${toolchain}-${board}"

command -v cmake >/dev/null
case "$toolchain" in
  arm-none-eabi-gcc) command -v arm-none-eabi-gcc >/dev/null ;;
  armclang) command -v armclang >/dev/null ;;
  atfe) command -v atfe >/dev/null ;;
  *) echo "unsupported toolchain: $toolchain" >&2; exit 2 ;;
esac

cmake -S "$consumer_root" -B "$build_dir" \
  -DNSX_TILEIO_ROOT="$repo_root" \
  -DNSX_TOOLCHAIN="$toolchain" \
  -DNSX_BOARD="$board"
cmake --build "$build_dir" --parallel 2
