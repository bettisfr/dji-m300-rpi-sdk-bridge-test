#!/usr/bin/env bash
set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly BINARY="${ROOT_DIR}/.build/dji_rpi_telemetry"

if [[ ! -c /dev/ttyUSB0 ]]; then
  echo "Missing /dev/ttyUSB0: connect the USB-to-TTL adapter." >&2
  exit 1
fi

if [[ ! -x "${BINARY}" ]]; then
  echo "Missing executable. Run ./scripts/build.sh first." >&2
  exit 1
fi

cd "${ROOT_DIR}"
exec "${BINARY}"
