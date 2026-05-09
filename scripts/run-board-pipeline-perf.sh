#!/usr/bin/env bash
set -euo pipefail

PACKAGE_DIR="${1:-build/board-native-debug-package}"
CONFIG_DIR="${2:-config/dev}"
FRAMES="${FRAMES:-300}"
INTERVAL="${INTERVAL:-30}"
BACKEND="${BACKEND:-}"
SINK="${SINK:-}"
CSV_PATH="${CSV_PATH:-perf/pipeline.csv}"
LOG_NAME="${LOG_NAME:-pipeline.log}"

CONFIG_FILE="${PACKAGE_DIR}/${CONFIG_DIR}/sentinel.yaml"
BACKUP_FILE="${CONFIG_FILE}.pipeline-perf.bak"
PERF_DIR="${PACKAGE_DIR}/data/dev/perf"
LOG_PATH="${PERF_DIR}/${LOG_NAME}"

if [[ ! -x "${PACKAGE_DIR}/bin/video_sentinel" ]]; then
    printf 'missing executable: %s\n' "${PACKAGE_DIR}/bin/video_sentinel" >&2
    exit 1
fi

if [[ ! -f "${CONFIG_FILE}" ]]; then
    printf 'missing config: %s\n' "${CONFIG_FILE}" >&2
    exit 1
fi

case "${BACKEND}" in
    ""|opencv|dvpp)
        ;;
    *)
        printf 'unsupported BACKEND: %s\n' "${BACKEND}" >&2
        exit 1
        ;;
esac

case "${SINK}" in
    ""|none|debug_image|mjpeg)
        ;;
    *)
        printf 'unsupported SINK: %s\n' "${SINK}" >&2
        exit 1
        ;;
esac

mkdir -p "${PERF_DIR}"
cp "${CONFIG_FILE}" "${BACKUP_FILE}"
restore_config() {
    mv "${BACKUP_FILE}" "${CONFIG_FILE}"
}
trap restore_config EXIT

if [[ -n "${BACKEND}" ]]; then
    perl -0pi -e "s/(pipeline:\\n\\s+backend: )\"(?:opencv|dvpp)\"/\${1}\"${BACKEND}\"/" \
        "${CONFIG_FILE}"
fi

if [[ -n "${SINK}" ]]; then
    sed -i -e "s/^  video_sink:.*/  video_sink: \"${SINK}\"/" "${CONFIG_FILE}"
fi

perl -0pi -e "s/(performance:\\n\\s+enabled: )(?:true|false)/\${1}true/" \
    "${CONFIG_FILE}"
sed -i \
    -e "s/^  log_interval_frames:.*/  log_interval_frames: ${INTERVAL}/" \
    -e "s|^  csv_path:.*|  csv_path: \"${CSV_PATH}\"|" \
    -e "s/^  max_frames:.*/  max_frames: ${FRAMES}/" \
    "${CONFIG_FILE}"

printf 'running single pipeline perf: package=%s config=%s frames=%s interval=%s csv=%s\n' \
    "${PACKAGE_DIR}" "${CONFIG_DIR}" "${FRAMES}" "${INTERVAL}" "${CSV_PATH}"
if [[ -n "${BACKEND}" ]]; then
    printf 'override backend=%s\n' "${BACKEND}"
fi
if [[ -n "${SINK}" ]]; then
    printf 'override sink=%s\n' "${SINK}"
fi

(
    cd "${PACKAGE_DIR}"
    ./bin/video_sentinel "${CONFIG_DIR}"
) 2>&1 | tee "${LOG_PATH}"

printf 'performance log: %s\n' "${LOG_PATH}"
printf 'performance csv: %s/%s\n' "${PACKAGE_DIR}/data/dev" "${CSV_PATH}"
