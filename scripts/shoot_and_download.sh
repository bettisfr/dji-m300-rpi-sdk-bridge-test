#!/usr/bin/env bash
set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SDK_TAG="${DJI_PSDK_VERSION:-3.9.2}"
readonly BINARY="${ROOT_DIR}/.build/${SDK_TAG}/dji_rpi_telemetry"
readonly OUTPUT_DIR="${1:-${ROOT_DIR}/photos}"

if [[ ! -c /dev/ttyUSB0 || ! -c /dev/ttyACM0 ]]; then
  echo "Both /dev/ttyUSB0 and /dev/ttyACM0 are required." >&2
  exit 1
fi

if [[ ! -x "${BINARY}" ]]; then
  echo "Missing executable. Run ./scripts/build.sh first." >&2
  exit 1
fi

cd "${ROOT_DIR}"
if [[ ! -r /dev/bus/usb/001/002 ]]; then
  echo "The DJI USB device is not readable by this user; run this command with sudo." >&2
  exit 1
fi
exec "${BINARY}" --shoot-download "${OUTPUT_DIR}"
