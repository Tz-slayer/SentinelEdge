#!/usr/bin/env bash
set -euo pipefail

PACKAGE_DIR="${1:-build/board-native-debug-package}"
CONFIG_DIR="${2:-config/perf}"
LOG_NAME="${LOG_NAME:-pipeline.log}"

CONFIG_FILE="${PACKAGE_DIR}/${CONFIG_DIR}/sentinel.yaml"
PERF_DIR="${PACKAGE_DIR}/data/perf"
LOG_PATH="${PERF_DIR}/${LOG_NAME}"

if [[ ! -x "${PACKAGE_DIR}/bin/video_sentinel" ]]; then
    printf 'missing executable: %s\n' "${PACKAGE_DIR}/bin/video_sentinel" >&2
    exit 1
fi

if [[ ! -f "${CONFIG_FILE}" ]]; then
    printf 'missing config: %s\n' "${CONFIG_FILE}" >&2
    exit 1
fi

mkdir -p "${PERF_DIR}"

printf 'running single pipeline perf: package=%s config=%s\n' \
    "${PACKAGE_DIR}" "${CONFIG_DIR}"
printf 'perf parameters are read from: %s\n' "${CONFIG_FILE}"

(
    cd "${PACKAGE_DIR}"
    ./bin/video_sentinel "${CONFIG_DIR}"
) 2>&1 | tee "${LOG_PATH}"

printf 'performance log: %s\n' "${LOG_PATH}"
printf 'performance csv is configured by performance.csv_path in %s\n' "${CONFIG_FILE}"
