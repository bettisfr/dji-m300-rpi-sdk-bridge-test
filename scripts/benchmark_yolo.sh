#!/usr/bin/env bash
set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly PYTHON="${HOME}/pyenv/bin/python"
readonly IMAGE="${1:-$(find "${ROOT_DIR}/photos" -maxdepth 1 -type f \( -iname '*.jpg' -o -iname '*.jpeg' \) -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)}"
readonly OUTPUT_DIR="${2:-${ROOT_DIR}/yolo-benchmark}"

if [[ ! -x "${PYTHON}" ]]; then
  echo "Python environment not found at ${PYTHON}" >&2
  exit 1
fi
if [[ -z "${IMAGE}" || ! -f "${IMAGE}" ]]; then
  echo "No input image found. Pass its path as the first argument." >&2
  exit 1
fi

cd "${ROOT_DIR}"
exec "${PYTHON}" scripts/benchmark_yolo.py \
  "${IMAGE}" \
  --output "${OUTPUT_DIR}" \
  --imgsz 640 \
  --runs 5
