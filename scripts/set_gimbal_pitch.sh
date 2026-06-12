#!/usr/bin/env bash
set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SDK_TAG="${DJI_PSDK_VERSION:-3.9.2}"
readonly BINARY="${ROOT_DIR}/.build/${SDK_TAG}/dji_rpi_telemetry"
readonly TARGET_PITCH="${1:--90}"

if [[ ! -c /dev/ttyUSB0 ]]; then
  echo "Missing /dev/ttyUSB0: connect the USB-to-TTL adapter." >&2
  exit 1
fi

if [[ ! -x "${BINARY}" ]]; then
  echo "Missing executable. Run ./scripts/build.sh first." >&2
  exit 1
fi

cd "${ROOT_DIR}"
exec "${BINARY}" --gimbal-pitch "${TARGET_PITCH}"
