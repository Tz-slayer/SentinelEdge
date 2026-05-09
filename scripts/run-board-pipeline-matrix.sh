#!/usr/bin/env bash
set -euo pipefail

CONFIG_PATH="${1:-config/perf/pipeline-matrix.conf}"

if [[ ! -f "${CONFIG_PATH}" ]]; then
    printf 'missing matrix config: %s\n' "${CONFIG_PATH}" >&2
    exit 1
fi

# shellcheck source=/dev/null
source "${CONFIG_PATH}"

PACKAGE_DIR="${PACKAGE_DIR:-build/board-native-debug-package}"
CONFIG_DIR="${CONFIG_DIR:-config/dev}"
FRAMES="${FRAMES:-300}"
INTERVAL="${INTERVAL:-30}"
WARMUP_FRAMES="${WARMUP_FRAMES:-0}"
CSV_DIR="${CSV_DIR:-perf/matrix}"
LOG_DIR="${LOG_DIR:-data/dev/perf/matrix}"
REPORT_PATH="${REPORT_PATH:-data/dev/perf/pipeline-matrix-report.md}"
REPORT_TITLE="${REPORT_TITLE:-SentinelEdge Pipeline 性能测试报告}"
OPENCV_MODEL_PATH="${OPENCV_MODEL_PATH:-models/yolo/yolo26n.om}"
OPENCV_OUTPUT_LAYOUT="${OPENCV_OUTPUT_LAYOUT:-NCHW}"
OPENCV_OUTPUT_DTYPE="${OPENCV_OUTPUT_DTYPE:-FP32}"
OPENCV_NORMALIZE="${OPENCV_NORMALIZE:-true}"
DVPP_MODEL_PATH="${DVPP_MODEL_PATH:-models/yolo/yolo26n_aipp_nv12.om}"
DVPP_OUTPUT_LAYOUT="${DVPP_OUTPUT_LAYOUT:-NV12}"
DVPP_OUTPUT_DTYPE="${DVPP_OUTPUT_DTYPE:-UINT8}"
DVPP_NORMALIZE="${DVPP_NORMALIZE:-false}"

if [[ ${#BACKENDS[@]} -eq 0 ]]; then
    printf 'BACKENDS must not be empty in %s\n' "${CONFIG_PATH}" >&2
    exit 1
fi

if [[ ${#BUFFER_MODES[@]} -eq 0 ]]; then
    printf 'BUFFER_MODES must not be empty in %s\n' "${CONFIG_PATH}" >&2
    exit 1
fi

SENTINEL_CONFIG="${PACKAGE_DIR}/${CONFIG_DIR}/sentinel.yaml"
CAMERA_CONFIG="${PACKAGE_DIR}/${CONFIG_DIR}/cameras.yaml"
SENTINEL_BACKUP="${SENTINEL_CONFIG}.pipeline-matrix.bak"
CAMERA_BACKUP="${CAMERA_CONFIG}.pipeline-matrix.bak"
LOG_ROOT="${PACKAGE_DIR}/${LOG_DIR}"
CSV_ROOT="${PACKAGE_DIR}/data/dev/${CSV_DIR}"
REPORT_OUTPUT="${PACKAGE_DIR}/${REPORT_PATH}"

if [[ ! -x "${PACKAGE_DIR}/bin/video_sentinel" ]]; then
    printf 'missing executable: %s\n' "${PACKAGE_DIR}/bin/video_sentinel" >&2
    exit 1
fi

if [[ ! -f "${SENTINEL_CONFIG}" ]]; then
    printf 'missing config: %s\n' "${SENTINEL_CONFIG}" >&2
    exit 1
fi

if [[ ! -f "${CAMERA_CONFIG}" ]]; then
    printf 'missing camera config: %s\n' "${CAMERA_CONFIG}" >&2
    exit 1
fi

for backend in "${BACKENDS[@]}"; do
    case "${backend}" in
        opencv|dvpp)
            ;;
        *)
            printf 'unsupported backend in BACKENDS: %s\n' "${backend}" >&2
            exit 1
            ;;
    esac
done

for buffer_mode in "${BUFFER_MODES[@]}"; do
    case "${buffer_mode}" in
        copy|loaned)
            ;;
        *)
            printf 'unsupported buffer mode in BUFFER_MODES: %s\n' "${buffer_mode}" >&2
            exit 1
            ;;
    esac
done

resolve_model_file() {
    local model_path="$1"
    if [[ "${model_path}" = /* ]]; then
        printf '%s\n' "${model_path}"
    else
        printf '%s/%s\n' "${PACKAGE_DIR}" "${model_path}"
    fi
}

require_model_file() {
    local model_path="$1"
    local model_file
    model_file="$(resolve_model_file "${model_path}")"
    if [[ ! -f "${model_file}" ]]; then
        printf 'missing model for matrix profile: %s\n' "${model_file}" >&2
        exit 1
    fi
}

apply_backend_profile() {
    local backend="$1"
    local model_path
    local output_layout
    local output_dtype
    local normalize

    case "${backend}" in
        opencv)
            model_path="${OPENCV_MODEL_PATH}"
            output_layout="${OPENCV_OUTPUT_LAYOUT}"
            output_dtype="${OPENCV_OUTPUT_DTYPE}"
            normalize="${OPENCV_NORMALIZE}"
            ;;
        dvpp)
            model_path="${DVPP_MODEL_PATH}"
            output_layout="${DVPP_OUTPUT_LAYOUT}"
            output_dtype="${DVPP_OUTPUT_DTYPE}"
            normalize="${DVPP_NORMALIZE}"
            ;;
        *)
            printf 'unsupported backend profile: %s\n' "${backend}" >&2
            exit 1
            ;;
    esac

    require_model_file "${model_path}"
    perl -0pi -e "s|(inference:\\n(?:\\s+.*\\n)*?\\s+model_path: )\"[^\"]*\"|\${1}\"${model_path}\"|" \
        "${SENTINEL_CONFIG}"
    perl -0pi -e "s|(preprocess:\\n(?:\\s+.*\\n)*?\\s+output_layout: )\"[^\"]*\"|\${1}\"${output_layout}\"|" \
        "${SENTINEL_CONFIG}"
    perl -0pi -e "s|(preprocess:\\n(?:\\s+.*\\n)*?\\s+output_dtype: )\"[^\"]*\"|\${1}\"${output_dtype}\"|" \
        "${SENTINEL_CONFIG}"
    perl -0pi -e "s#(preprocess:\\n(?:\\s+.*\\n)*?\\s+normalize: )(?:true|false)#\${1}${normalize}#" \
        "${SENTINEL_CONFIG}"
}

mkdir -p "${LOG_ROOT}" "${CSV_ROOT}"
cp "${SENTINEL_CONFIG}" "${SENTINEL_BACKUP}"
cp "${CAMERA_CONFIG}" "${CAMERA_BACKUP}"

restore_config() {
    mv "${SENTINEL_BACKUP}" "${SENTINEL_CONFIG}"
    mv "${CAMERA_BACKUP}" "${CAMERA_CONFIG}"
}
trap restore_config EXIT

for backend in "${BACKENDS[@]}"; do
    for buffer_mode in "${BUFFER_MODES[@]}"; do
        run_name="backend-${backend}-buffer-${buffer_mode}"
        csv_path="${CSV_DIR}/${run_name}.csv"
        log_path="${LOG_ROOT}/${run_name}.log"

        cp "${SENTINEL_BACKUP}" "${SENTINEL_CONFIG}"
        cp "${CAMERA_BACKUP}" "${CAMERA_CONFIG}"

        perl -0pi -e "s/(pipeline:\\n\\s+backend: )\"(?:opencv|dvpp)\"/\${1}\"${backend}\"/" \
            "${SENTINEL_CONFIG}"
        apply_backend_profile "${backend}"
        perl -0pi -e "s/(performance:\\n\\s+enabled: )(?:true|false)/\${1}true/" \
            "${SENTINEL_CONFIG}"
        sed -i \
            -e 's/^  video_sink:.*/  video_sink: "none"/' \
            -e "s/^  log_interval_frames:.*/  log_interval_frames: ${INTERVAL}/" \
            -e "s|^  csv_path:.*|  csv_path: \"${csv_path}\"|" \
            -e "s/^  max_frames:.*/  max_frames: ${FRAMES}/" \
            "${SENTINEL_CONFIG}"
        sed -i -e "s/^    buffer_mode:.*/    buffer_mode: \"${buffer_mode}\"/" \
            "${CAMERA_CONFIG}"

        printf 'running pipeline matrix: backend=%s buffer_mode=%s sink=none frames=%s\n' \
            "${backend}" "${buffer_mode}" "${FRAMES}"
        (
            cd "${PACKAGE_DIR}"
            ./bin/video_sentinel "${CONFIG_DIR}"
        ) 2>&1 | tee "${log_path}"
    done
done

python3 scripts/generate-pipeline-perf-report.py \
    --csv-dir "${CSV_ROOT}" \
    --output "${REPORT_OUTPUT}" \
    --title "${REPORT_TITLE}" \
    --warmup "${WARMUP_FRAMES}"

printf 'matrix logs: %s\n' "${LOG_ROOT}"
printf 'matrix csv: %s\n' "${CSV_ROOT}"
printf 'matrix report: %s\n' "${REPORT_OUTPUT}"
