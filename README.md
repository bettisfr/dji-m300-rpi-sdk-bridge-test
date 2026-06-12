# DJI M300 RTK on Raspberry Pi 5

Minimal C application for reading Matrice 300 RTK telemetry and controlling an
H20 gimbal from a Raspberry Pi 5 through the DJI E-Port Developer Kit.

The configuration documented here was verified with:

- DJI Matrice 300 RTK, aircraft firmware `3.4.18.68`;
- Zenmuse H20 on payload position 1;
- Raspberry Pi 5;
- DJI E-Port Developer Kit connected to the M300 OSDK/Extension Port;
- CP2102 USB-to-TTL adapter at 460800 baud;
- DJI Payload SDK `3.9.2`.

## Required SDK Version

Use **Payload SDK 3.9.2** for this M300 configuration.

| PSDK | Verified result |
| --- | --- |
| 3.9.2 | Telemetry and gimbal control work |
| 3.8.1 | Telemetry works |
| 3.13.1 | Core starts, but telemetry subscriptions are rejected |
| 3.16.0 | `DjiCore_Init` fails with `Unknown mount position type` |

The 3.16.0 failure was reproduced with DJI's official Raspberry Pi sample. It
is therefore not caused by this application's platform initialization.

The build scripts use 3.9.2 by default. Do not upgrade the SDK without testing
the complete telemetry and gimbal workflow on the aircraft.

## Working Connection

Use the **M300 OSDK port**, not a gimbal/PSDK payload port.

Connect everything while the aircraft and Raspberry Pi are powered off.

```text
M300 OSDK port
    |
    | DJI coaxial cable, with the A/B ends in the correct orientation
    v
E-Port Developer Kit
    |
    +-- UART TXD ----> CP2102 RXD
    +-- UART RXD ----> CP2102 TXD
    +-- UART GND ----> CP2102 GND
    |
    +-- DEVICE USB-C/data ----> Raspberry Pi USB-A host port
    |                           using a USB-A to USB-C data cable
    |
    +-- central yellow XT30 --> DJI XT30 to USB-C power cable
                                |
                                +--> Raspberry Pi USB-C power input

CP2102 USB connector ----------> Raspberry Pi USB-A port
```

Important details:

- TX and RX must be crossed: `TXD -> RXD` and `RXD -> TXD`.
- Connect GND between the CP2102 and E-Port.
- Do not connect the E-Port `5V0` or `3V3` pins to the adapter or Raspberry Pi.
- Set the E-Port USB selector to the **Device** position for the M300 OSDK
  connection. The Raspberry Pi acts as USB host.
- The cable from the E-Port `DEVICE` USB-C port must carry data. A charge-only
  cable is insufficient.
- Since the Raspberry Pi 5 has one USB-C port, reserve it for power and connect
  the E-Port `DEVICE` data port to a USB-A port with a USB-A to USB-C data
  cable.
- In the verified setup, the Pi is powered from the E-Port's central yellow
  XT30 power output using the DJI `XT30 to USB-C Power Cable`.
- Use the intended DJI power cable; never wire an XT30 output directly to the
  Raspberry Pi USB-C input.

The DJI USB connection and CP2102 are both required:

```text
/dev/ttyUSB0  UART0 from the CP2102
/dev/ttyACM0  UART1 exposed by the M300 over the E-Port USB data connection
```

UART0 is enough for core communication and telemetry. UART1 is also required
for aircraft services such as `DjiGimbalManager`; without `/dev/ttyACM0`,
telemetry works but gimbal commands time out with `0xE1`.

## Connection Check

After powering the aircraft, E-Port, and Raspberry Pi:

```bash
ls -l /dev/ttyUSB0 /dev/ttyACM0
lsusb
```

Both serial devices must exist. The DJI USB device used during testing appeared
as vendor/product `2ca3:001f`, and the CP2102 as `10c4:ea60`.

The application does not need `sudo` when the user can access both serial
devices. Check membership with:

```bash
groups
```

If necessary:

```bash
sudo usermod -aG dialout "$USER"
```

Log out and back in after changing groups.

## Power Check

Check Raspberry Pi undervoltage and throttling with:

```bash
vcgencmd get_throttled
```

The expected healthy result is:

```text
throttled=0x0
```

This means no current or historical undervoltage/throttling event has occurred
since boot. The complete telemetry and gimbal tests were performed with
`throttled=0x0`, so the SDK compatibility failures above were not caused by
undervoltage.

Occasional kernel messages such as:

```text
cp210x ttyUSB0: failed set request 0x12 status: -110
```

refer to a CP2102 USB control-request timeout. They are not, by themselves,
evidence of Raspberry Pi undervoltage. Check cabling and USB stability if they
appear during normal operation or communication is interrupted.

## Setup

Install the build dependencies:

```bash
cd ~/dji-rpi
./scripts/setup.sh
```

Create the private application configuration:

```bash
cp config/app.env.example config/app.env
chmod 600 config/app.env
```

Fill `config/app.env` with the application data created in the DJI developer
portal:

```text
DJI_APP_NAME
DJI_APP_ID
DJI_APP_KEY
DJI_APP_LICENSE
DJI_DEVELOPER_ACCOUNT
```

The working transport defaults are:

```text
DJI_BAUD_RATE=460800
DJI_UART_DEVICE=/dev/ttyUSB0
DJI_UART_SECONDARY_DEVICE=/dev/ttyACM0
```

`config/app.env` contains secrets and is ignored by Git.

## Build

```bash
./scripts/build.sh
```

The script downloads the exact PSDK 3.9.2 tag into:

```text
.cache/Payload-SDK-3.9.2
```

and creates:

```text
.build/3.9.2/dji_rpi_telemetry
```

Another version can be built in a separate directory for comparison:

```bash
DJI_PSDK_VERSION=3.8.1 ./scripts/build.sh
```

## Telemetry

Run:

```bash
./scripts/run_telemetry.sh
```

The application prints one CSV row per second containing:

- aircraft model and configured firmware;
- aircraft bearing calculated from its quaternion;
- H20 gimbal pitch, roll, and absolute yaw;
- aggregate battery percentage and voltage.

Example:

```text
timestamp_ms,drone_model,firmware,bearing_deg,gimbal_roll_deg,gimbal_pitch_deg,gimbal_yaw_deg,battery_percent,battery_voltage_v
1975800,"Matrice 300 RTK","3.4.18.68",279.4,0.0,0.0,-75.9,21,44.406
```

Stop with `Ctrl-C`.

## Gimbal Control

Before moving the gimbal, keep the aircraft stationary and ensure the gimbal has
enough clearance.

Start the interactive console:

```bash
./scripts/gimbal_console.sh
```

The menu remains connected to PSDK while accepting multiple commands:

```text
Current: pitch -29.9, roll 10.0, yaw -105.8 deg
1 pitch | 2 roll | 3 yaw | q quit
Select axis:
Relative movement in degrees:
```

Select:

- `1` for pitch;
- `2` for roll;
- `3` for yaw;
- `q` to exit.

Values are **relative movements**. For example, selecting yaw and entering `15`
adds 15 degrees to the current yaw; entering `-15` moves it back.

The console reads and prints all three angles before and after each movement.
Pitch, roll, and yaw were all verified on the H20.

For a one-shot absolute pitch target:

```bash
./scripts/set_gimbal_pitch.sh -20
```

This command reads the current pitch, calculates the required relative
movement, and finishes at the requested pitch target.

## PSDK 3.9.2 Shutdown Note

With the M300 dual-UART connection, `DjiCore_DeInit()` in PSDK 3.9.2 races the
UART1 receive task against destruction of its internal message queue. The
visible symptoms are repeated messages such as:

```text
DjiMsgq_Send: semaphore wait timeout
DjiLinker_RecvTask: send msg to queue error
```

They occur after commands have completed and do not indicate a failed gimbal
movement. This CLI intentionally skips the defective `DjiCore_DeInit()` call on
normal process exit; Linux releases the process file descriptors, threads, and
memory. The interactive console now exits cleanly with `q`.

## Troubleshooting

`DjiCore_Init` cannot identify the aircraft:

- confirm the E-Port is connected to the M300 OSDK port;
- verify the coaxial cable A/B orientation;
- verify crossed TX/RX and common GND;
- confirm `/dev/ttyUSB0`;
- use PSDK 3.9.2 and 460800 baud.

Telemetry works but gimbal commands return `0xE1`:

- confirm the E-Port USB-C connection is a data connection;
- confirm the E-Port selector is on `Device`;
- confirm `/dev/ttyACM0` exists;
- confirm both UART devices are accessible by the current user.

The sample only works with `sudo`:

- inspect permissions with `ls -l /dev/ttyUSB0 /dev/ttyACM0`;
- add the user to `dialout`;
- do not run this application as root when the serial permissions are correct.

## Source Layout

- `src/main.c`: platform setup, PSDK initialization, and CLI dispatch.
- `src/telemetry.c`: topic subscriptions and CSV telemetry output.
- `src/gimbal_control.c`: one-shot pitch and interactive three-axis control.
- `src/hal_uart.c`: Linux HAL for `/dev/ttyUSB0` and `/dev/ttyACM0`.
- `CMakeLists.txt`: minimal executable linked to DJI's precompiled library.
- `scripts/build.sh`: exact SDK download, private config generation, and build.
- `scripts/run_telemetry.sh`: telemetry launcher.
- `scripts/gimbal_console.sh`: interactive gimbal launcher.
- `scripts/set_gimbal_pitch.sh`: one-shot pitch launcher.
