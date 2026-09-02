#!/usr/bin/env bash
set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SDK_TAG="${DJI_PSDK_VERSION:-3.9.2}"
readonly SDK_DIR="${ROOT_DIR}/.cache/Payload-SDK-${SDK_TAG}"
readonly BUILD_DIR="${ROOT_DIR}/.build/${SDK_TAG}"
readonly APP_ENV="${ROOT_DIR}/config/app.env"
readonly GENERATED_DIR="${BUILD_DIR}/generated"

if [[ ! -f "${APP_ENV}" ]]; then
  echo "Missing ${APP_ENV}; copy config/app.env.example and fill it in." >&2
  exit 1
fi

set -a
# shellcheck disable=SC1090
source "${APP_ENV}"
set +a

DJI_UART_SECONDARY_DEVICE="${DJI_UART_SECONDARY_DEVICE:-/dev/serial/by-id/usb-DJI_e1e_123456789ABCDEF-if04}"

required=(
  DJI_APP_NAME
  DJI_APP_ID
  DJI_APP_KEY
  DJI_APP_LICENSE
  DJI_DEVELOPER_ACCOUNT
  DJI_BAUD_RATE
  DJI_UART_DEVICE
  DJI_AIRCRAFT_FIRMWARE
)

for name in "${required[@]}"; do
  if [[ -z "${!name:-}" ]]; then
    echo "Missing ${name} in ${APP_ENV}" >&2
    exit 1
  fi
done

if [[ ! -d "${SDK_DIR}/.git" ]]; then
  mkdir -p "$(dirname "${SDK_DIR}")"
  git clone --branch "${SDK_TAG}" --depth 1 \
    https://github.com/dji-sdk/Payload-SDK.git "${SDK_DIR}"
else
  git -C "${SDK_DIR}" fetch --depth 1 origin "refs/tags/${SDK_TAG}:refs/tags/${SDK_TAG}"
fi

git -C "${SDK_DIR}" reset --hard "${SDK_TAG}" >/dev/null

escape_c_string() {
  printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

mkdir -p "${GENERATED_DIR}"
cat >"${GENERATED_DIR}/dji_sdk_app_info.h" <<EOF
#ifndef DJI_SDK_APP_INFO_H
#define DJI_SDK_APP_INFO_H

#define USER_APP_NAME "$(escape_c_string "${DJI_APP_NAME}")"
#define USER_APP_ID "$(escape_c_string "${DJI_APP_ID}")"
#define USER_APP_KEY "$(escape_c_string "${DJI_APP_KEY}")"
#define USER_APP_LICENSE "$(escape_c_string "${DJI_APP_LICENSE}")"
#define USER_DEVELOPER_ACCOUNT "$(escape_c_string "${DJI_DEVELOPER_ACCOUNT}")"
#define USER_BAUD_RATE "$(escape_c_string "${DJI_BAUD_RATE}")"
#define USER_UART_DEVICE "$(escape_c_string "${DJI_UART_DEVICE}")"
#define USER_UART_SECONDARY_DEVICE "$(escape_c_string "${DJI_UART_SECONDARY_DEVICE}")"
#define USER_AIRCRAFT_FIRMWARE "$(escape_c_string "${DJI_AIRCRAFT_FIRMWARE}")"

#endif
EOF

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DDJI_PSDK_DIR="${SDK_DIR}"
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo "Built PSDK ${SDK_TAG}: ${BUILD_DIR}/dji_rpi_telemetry"
