#!/usr/bin/env bash
set -euo pipefail

PACKAGE_DIR="${1:-build/board-native-debug-package}"
CONFIG_DIR="${2:-config/dev}"
FRAMES="${FRAMES:-300}"
INTERVAL="${INTERVAL:-30}"
SINKS="${SINKS:-none debug_image mjpeg}"
BACKENDS="${BACKENDS:-opencv}"

CONFIG_FILE="${PACKAGE_DIR}/${CONFIG_DIR}/sentinel.yaml"
BACKUP_FILE="${CONFIG_FILE}.perf-matrix.bak"
PERF_DIR="${PACKAGE_DIR}/data/dev/perf"

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

for backend in ${BACKENDS}; do
    case "${backend}" in
        opencv|dvpp)
            ;;
        *)
            printf 'unsupported pipeline backend in BACKENDS: %s\n' "${backend}" >&2
            exit 1
            ;;
    esac

    for sink in ${SINKS}; do
        case "${sink}" in
            none|debug_image|mjpeg)
                ;;
            *)
                printf 'unsupported sink in SINKS: %s\n' "${sink}" >&2
                exit 1
                ;;
        esac

        csv_path="perf/pipeline-${backend}-${sink}.csv"
        log_path="${PERF_DIR}/run-${backend}-${sink}.log"

        perl -0pi -e "s/(pipeline:\\n\\s+backend: )\"(?:opencv|dvpp)\"/\${1}\"${backend}\"/" \
            "${CONFIG_FILE}"

        sed -i \
            -e "s/^  video_sink:.*/  video_sink: \"${sink}\"/" \
            -e "s/^  log_interval_frames:.*/  log_interval_frames: ${INTERVAL}/" \
            -e "s|^  csv_path:.*|  csv_path: \"${csv_path}\"|" \
            -e "s/^  max_frames:.*/  max_frames: ${FRAMES}/" \
            "${CONFIG_FILE}"

        printf 'running backend=%s sink=%s frames=%s interval=%s\n' \
            "${backend}" "${sink}" "${FRAMES}" "${INTERVAL}"
        (
            cd "${PACKAGE_DIR}"
            ./bin/video_sentinel "${CONFIG_DIR}"
        ) 2>&1 | tee "${log_path}"
    done
done

printf 'performance logs: %s\n' "${PERF_DIR}"
