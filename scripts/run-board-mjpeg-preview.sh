#!/usr/bin/env bash
set -euo pipefail

PACKAGE_DIR="${1:-build/board-native-debug-package}"
CONFIG_DIR="${2:-config/dev}"
FRAMES="${FRAMES:-300}"

CONFIG_FILE="${PACKAGE_DIR}/${CONFIG_DIR}/sentinel.yaml"
BACKUP_FILE="${CONFIG_FILE}.mjpeg-preview.bak"

if [[ ! -x "${PACKAGE_DIR}/bin/video_sentinel" ]]; then
    printf 'missing executable: %s\n' "${PACKAGE_DIR}/bin/video_sentinel" >&2
    exit 1
fi

if [[ ! -f "${CONFIG_FILE}" ]]; then
    printf 'missing config: %s\n' "${CONFIG_FILE}" >&2
    exit 1
fi

cp "${CONFIG_FILE}" "${BACKUP_FILE}"
restore_config() {
    mv "${BACKUP_FILE}" "${CONFIG_FILE}"
}
trap restore_config EXIT

sed -i \
    -e 's/^  video_sink:.*/  video_sink: "mjpeg"/' \
    -e "s/^  max_frames:.*/  max_frames: ${FRAMES}/" \
    "${CONFIG_FILE}"

printf 'MJPEG preview: http://<board-ip>:8081/stream\n'
(
    cd "${PACKAGE_DIR}"
    ./bin/video_sentinel "${CONFIG_DIR}"
)
