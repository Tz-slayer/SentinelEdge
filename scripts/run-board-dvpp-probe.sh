#!/usr/bin/env bash
set -euo pipefail

PACKAGE_DIR="${1:-build/board-native-debug-package}"
JPEG_PATH="${2:-}"
DEVICE_ID="${DEVICE_ID:-0}"
WIDTH="${WIDTH:-640}"
HEIGHT="${HEIGHT:-640}"

if [[ ! -x "${PACKAGE_DIR}/bin/sentinel_dvpp_probe" ]]; then
    printf 'missing executable: %s\n' "${PACKAGE_DIR}/bin/sentinel_dvpp_probe" >&2
    exit 1
fi

args=(
    --device-id "${DEVICE_ID}"
    --width "${WIDTH}"
    --height "${HEIGHT}"
)

if [[ -n "${JPEG_PATH}" ]]; then
    args+=(--jpeg "${JPEG_PATH}")
fi

(
    cd "${PACKAGE_DIR}"
    ./bin/sentinel_dvpp_probe "${args[@]}"
)
