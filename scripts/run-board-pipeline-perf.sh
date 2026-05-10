#!/usr/bin/env bash
set -euo pipefail

PACKAGE_DIR="${1:-build/board-native-debug-package}"
CONFIG_DIR="${2:-config/dev}"
FRAMES="${FRAMES:-300}"
INTERVAL="${INTERVAL:-30}"
DETECT_FPS="${DETECT_FPS:-30}"
STREAM_SLOTS="${STREAM_SLOTS:-2}"
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

mkdir -p "${PERF_DIR}"
cp "${CONFIG_FILE}" "${BACKUP_FILE}"
restore_config() {
    mv "${BACKUP_FILE}" "${CONFIG_FILE}"
}
trap restore_config EXIT

perl -0pi -e "s/(pipeline:\\n\\s+backend: )\"[^\"]*\"/\${1}\"dvpp\"/" \
    "${CONFIG_FILE}"
perl -0pi -e "s/(performance:\\n\\s+enabled: )(?:true|false)/\${1}true/" \
    "${CONFIG_FILE}"
sed -i \
    -e 's/^  level:.*/  level: "info"/' \
    -e 's/^  video_sink:.*/  video_sink: "none"/' \
    -e "s/^  log_interval_frames:.*/  log_interval_frames: ${INTERVAL}/" \
    -e "s|^  csv_path:.*|  csv_path: \"${CSV_PATH}\"|" \
    -e "s/^  max_frames:.*/  max_frames: ${FRAMES}/" \
    -e "s/^  detect_fps:.*/  detect_fps: ${DETECT_FPS}/" \
    -e "s/^  stream_slots:.*/  stream_slots: ${STREAM_SLOTS}/" \
    "${CONFIG_FILE}"

printf 'running single pipeline perf: package=%s config=%s frames=%s interval=%s detect_fps=%s stream_slots=%s csv=%s\n' \
    "${PACKAGE_DIR}" "${CONFIG_DIR}" "${FRAMES}" "${INTERVAL}" "${DETECT_FPS}" "${STREAM_SLOTS}" "${CSV_PATH}"
printf 'fixed profile: backend=dvpp buffer_mode=loaned sink=none\n'

(
    cd "${PACKAGE_DIR}"
    ./bin/video_sentinel "${CONFIG_DIR}"
) 2>&1 | tee "${LOG_PATH}"

printf 'performance log: %s\n' "${LOG_PATH}"
printf 'performance csv: %s/%s\n' "${PACKAGE_DIR}/data/dev" "${CSV_PATH}"
