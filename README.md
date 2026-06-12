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
| 3.9.2 | Telemetry, gimbal control, H20 photo capture, and media download work |
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

Telemetry, gimbal control, and commands that only use the serial transports do
not need `sudo` when the user can access both serial devices. Check membership
with:

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

## Camera Control

The camera commands target the H20 mounted on payload position 1.

To take one photo and leave it on the H20 storage:

```bash
./scripts/shoot_photo.sh
```

The command identifies the camera, switches it to single-photo mode, and sends
one shutter command.

To take one photo and download the newest media file to the Raspberry Pi:

```bash
sudo ./scripts/shoot_and_download.sh
```

The default destination is:

```text
~/dji-rpi/photos/
```

A different destination can be passed as the first argument:

```bash
sudo ./scripts/shoot_and_download.sh /path/to/output
```

The complete verified sequence is:

1. initialize the H20 camera manager;
2. select single-photo mode;
3. trigger the shutter;
4. wait for the H20 to store the image;
5. obtain downloader rights;
6. request the camera media list;
7. select the file with the highest media index;
8. download it over the E-Port USB Bulk channel.

On the tested M300/E-Port connection, PSDK 3.9.2 selected USB device
`2ca3:001f`, interface `3`, endpoint `0x84` IN, and endpoint `0x03` OUT. These
values are supplied by PSDK to the USB HAL and are not hard-coded by the
application.

Media download uses `libusb`, installed by `scripts/setup.sh`. With the default
Raspberry Pi USB-device permissions, opening the DJI Bulk interface requires
root, hence `sudo` for `shoot_and_download.sh`. The serial-only commands do not
require root when the user belongs to `dialout`.

The downloaded files are ignored by Git through the `photos/` entry in
`.gitignore`. The verified H20 output was a JPEG with EXIF metadata at
`5184x3888`.

## YOLO Benchmark

The benchmark uses the existing Python environment at `~/pyenv` and compares
the official COCO-pretrained YOLO11 detection models `n`, `s`, `m`, and `l` on
the same H20 image.

Run it with:

```bash
./scripts/benchmark_yolo.sh
```

By default, the script selects the newest JPEG in `photos/`, uses a `640x640`
inference size, performs one warm-up and five measured runs per model, and
writes results to:

```text
yolo-benchmark/results.json
yolo-benchmark/results.csv
yolo-benchmark/yolo11*-annotated.jpg
```

An explicit image and output directory can be supplied:

```bash
./scripts/benchmark_yolo.sh /path/to/photo.jpg /path/to/results
```

The measured wall time includes image loading, resize/preprocessing, model
inference, and postprocessing. The separate `inference_mean_ms` field reports
the model execution time measured by Ultralytics. Model loading and the warm-up
are reported separately and are excluded from the averages.

The initial verified Raspberry Pi 5 CPU results at input size `640`, using four
PyTorch threads and five runs, were:

| Model | Mean wall time | Mean inference | Approx. rate |
| --- | ---: | ---: | ---: |
| YOLO11n | 473 ms | 290 ms | 2.11 images/s |
| YOLO11s | 959 ms | 773 ms | 1.04 images/s |
| YOLO11m | 2448 ms | 2262 ms | 0.41 images/s |
| YOLO11l | 3020 ms | 2835 ms | 0.33 images/s |

The test completed at `53.2 C`, with no swap usage and
`vcgencmd get_throttled` equal to `0x0`. Zero detections on a test image means
that the generic COCO model did not recognize a supported class in that scene;
it does not indicate an inference failure.

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
- serial-only commands should not need root when permissions are correct;
- camera media download may still require `sudo` to claim the DJI USB Bulk
  interface.

Photo capture succeeds but download reports `Usb bulk and socket handler is
null`:

- rebuild the current application, which registers `src/hal_usb_bulk.c`;
- confirm the E-Port `DEVICE` USB-C data cable is connected to a Pi USB-A port;
- confirm `lsusb` shows the DJI device;
- install `libusb-1.0-0-dev` with `scripts/setup.sh`;
- run `sudo ./scripts/shoot_and_download.sh`.

## Source Layout

- `src/main.c`: platform setup, PSDK initialization, and CLI dispatch.
- `src/telemetry.c`: topic subscriptions and CSV telemetry output.
- `src/gimbal_control.c`: one-shot pitch and interactive three-axis control.
- `src/camera_control.c`: H20 photo capture, media-list query, and file download.
- `src/hal_uart.c`: Linux HAL for `/dev/ttyUSB0` and `/dev/ttyACM0`.
- `src/hal_usb_bulk.c`: libusb transport used for H20 media downloads.
- `CMakeLists.txt`: minimal executable linked to DJI's precompiled library.
- `scripts/build.sh`: exact SDK download, private config generation, and build.
- `scripts/run_telemetry.sh`: telemetry launcher.
- `scripts/gimbal_console.sh`: interactive gimbal launcher.
- `scripts/set_gimbal_pitch.sh`: one-shot pitch launcher.
- `scripts/shoot_photo.sh`: single-photo launcher.
- `scripts/shoot_and_download.sh`: capture and download the newest H20 photo.
- `scripts/benchmark_yolo.sh`: benchmark YOLO11 n/s/m/l on an H20 image.
- `scripts/benchmark_yolo.py`: timed Ultralytics inference and result export.
