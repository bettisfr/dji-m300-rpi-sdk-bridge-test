# DJI M300 RTK Telemetry on Raspberry Pi 5

Minimal C application using DJI Payload SDK 3.8.1 to read telemetry from a
Matrice 300 RTK through an E-Port Developer Kit.

The application prints once per second:

- aircraft model and configured firmware version;
- aircraft bearing;
- H20 gimbal roll, pitch and absolute yaw;
- aggregate battery percentage and voltage.

It links directly to DJI's precompiled `libpayloadsdk.a`. DJI sample
applications are neither compiled nor patched.

## Hardware

Current UART connection:

```text
Raspberry Pi USB -> CP2102 USB-to-TTL adapter
CP2102 TXD       -> E-Port RXD
CP2102 RXD       -> E-Port TXD
CP2102 GND       -> E-Port GND
```

Do not connect the E-Port `5V0` or `3V3` pins to the adapter or Raspberry Pi.
The serial device is `/dev/ttyUSB0` at 460800 baud.

## Setup

On the Raspberry Pi:

```bash
cd ~/dji-rpi
./scripts/setup.sh
cp config/app.env.example config/app.env
chmod 600 config/app.env
```

Fill `config/app.env` with the DJI application credentials. Keep this file
private; it is ignored by Git.

## Build

```bash
./scripts/build.sh
```

The build downloads Payload SDK 3.8.1 into `.cache/Payload-SDK` and creates:

```text
.build/dji_rpi_telemetry
```

Only five objects are compiled: the three project sources and DJI's Linux OSAL
and filesystem adapters.

## Run

With the drone and E-Port powered:

```bash
./scripts/run_telemetry.sh
```

Stop with `Ctrl-C`. Output is CSV:

```text
timestamp_ms,drone_model,firmware,bearing_deg,gimbal_roll_deg,gimbal_pitch_deg,gimbal_yaw_deg,battery_percent,battery_voltage_v
926620,"Matrice 300 RTK","3.4.18.68",277.4,0.0,1.0,-84.3,34,44.811
```

## Source Layout

- `src/main.c`: PSDK and platform initialization.
- `src/telemetry.c`: topic subscriptions and CSV output.
- `src/hal_uart.c`: Linux UART adapter for `/dev/ttyUSB0`.
- `CMakeLists.txt`: minimal executable definition.
- `scripts/build.sh`: SDK download, credential header generation and build.
