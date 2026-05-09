#!/usr/bin/env bash
set -euo pipefail

cmake --preset board-native-debug
cmake --build --preset board-native-debug -j"$(nproc)"
cmake --install build/board-native-debug

printf 'board native debug package: %s\n' "build/board-native-debug-package"
