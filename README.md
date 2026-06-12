# DJI M300 RTK on Raspberry Pi 5

Minimal application for:

- reading DJI Matrice 300 RTK telemetry;
- controlling a Zenmuse H20 gimbal;
- taking and downloading H20 photos;
- benchmarking YOLO inference on the downloaded images.

Verified hardware and software:

- DJI Matrice 300 RTK, firmware `3.4.18.68`;
- Zenmuse H20 on payload position 1;
- Raspberry Pi 5;
- DJI E-Port Developer Kit;
- CP2102 USB-to-TTL adapter;
- DJI Payload SDK **3.9.2**.

Use PSDK 3.9.2. Newer releases tested with this M300 configuration did not
provide the same working telemetry and control path.

## Connections

Connect everything while the aircraft and Raspberry Pi are powered off.

```text
M300 OSDK port
    |
    | DJI coaxial cable, with A/B ends in the correct orientation
    v
E-Port Developer Kit
    |
    +-- TXD ----------> CP2102 RXD
    +-- RXD ----------> CP2102 TXD
    +-- GND ----------> CP2102 GND
    |
    +-- DEVICE USB-C -> Raspberry Pi USB-A
    |                   with a USB-A to USB-C data cable
    |
    +-- central XT30 -> DJI XT30 to USB-C power cable
                        |
                        +--> Raspberry Pi USB-C power input

CP2102 USB -----------> Raspberry Pi USB-A
```

Requirements:

- use the M300 **OSDK** port;
- cross TX and RX;
- connect the common GND;
- do not connect the E-Port `5V0` or `3V3` pins;
- set the E-Port USB selector to **Device**;
- use a data-capable cable between E-Port and Raspberry Pi;
- use the DJI XT30-to-USB-C cable to power the Raspberry Pi.

The two communication devices must be present:

```text
/dev/ttyUSB0  CP2102 UART
/dev/ttyACM0  E-Port USB UART
```

Check the connection with:

```bash
ls -l /dev/ttyUSB0 /dev/ttyACM0
lsusb
vcgencmd get_throttled
```

A healthy power result is `throttled=0x0`.

Add the user to `dialout` if the serial devices are not accessible:

```bash
sudo usermod -aG dialout "$USER"
```

Log out and back in after changing groups.

## Setup

Install dependencies:

```bash
cd ~/dji-rpi
./scripts/setup.sh
```

Create the private DJI configuration:

```bash
cp config/app.env.example config/app.env
chmod 600 config/app.env
```

Fill `config/app.env` with the application credentials from the DJI developer
portal. The verified transport configuration is:

```text
DJI_BAUD_RATE=460800
DJI_UART_DEVICE=/dev/ttyUSB0
DJI_UART_SECONDARY_DEVICE=/dev/ttyACM0
```

Build the application:

```bash
./scripts/build.sh
```

The script downloads PSDK 3.9.2 and creates:

```text
.build/3.9.2/dji_rpi_telemetry
```

## Telemetry

```bash
./scripts/run_telemetry.sh
```

The application prints one CSV row per second containing:

- aircraft model and firmware;
- aircraft bearing;
- gimbal pitch, roll, and yaw;
- aggregate battery percentage and voltage.

Stop with `Ctrl-C`.

## Gimbal

Keep the aircraft stationary and ensure the gimbal has enough clearance.

Interactive relative control:

```bash
./scripts/gimbal_console.sh
```

Select `1` for pitch, `2` for roll, `3` for yaw, or `q` to exit. Entered angles
are relative movements.

Set an absolute pitch target:

```bash
./scripts/set_gimbal_pitch.sh -20
```

## Photos

Take a photo and leave it on the H20 storage:

```bash
./scripts/shoot_photo.sh
```

Take a photo and download the newest H20 media file:

```bash
sudo ./scripts/shoot_and_download.sh
```

The default output directory is:

```text
~/dji-rpi/photos/
```

Pass a different destination as the first argument:

```bash
sudo ./scripts/shoot_and_download.sh /path/to/output
```

The download uses the E-Port USB Bulk interface through `libusb`. With the
default Raspberry Pi USB permissions, this operation requires `sudo`.
Telemetry, gimbal control, and the shutter-only command do not require root
when serial permissions are configured correctly.

## YOLO Benchmark

The benchmark uses `~/pyenv` and the official COCO-pretrained YOLO11 detection
models `n`, `s`, `m`, and `l`.

```bash
./scripts/benchmark_yolo.sh
```

It selects the newest JPEG in `photos/`, runs at input size `640`, performs one
warm-up and five measured inferences per model, and writes:

```text
yolo-benchmark/results.json
yolo-benchmark/results.csv
yolo-benchmark/yolo11*-annotated.jpg
```

An explicit image and output directory can be supplied:

```bash
./scripts/benchmark_yolo.sh /path/to/photo.jpg /path/to/results
```

Measured Raspberry Pi 5 CPU performance:

| Model | Total per image | Model inference |
| --- | ---: | ---: |
| YOLO11n | 0.47 s | 0.29 s |
| YOLO11s | 0.96 s | 0.77 s |
| YOLO11m | 2.45 s | 2.26 s |
| YOLO11l | 3.02 s | 2.83 s |

These times exclude model loading and warm-up. The test used four PyTorch
threads and completed without throttling.

## Troubleshooting

`DjiCore_Init` cannot identify the aircraft:

- confirm the E-Port is connected to the M300 OSDK port;
- verify the coaxial A/B orientation;
- verify crossed TX/RX and common GND;
- confirm `/dev/ttyUSB0`;
- confirm PSDK 3.9.2 and baud rate 460800.

Gimbal commands return `0xE1`:

- confirm the E-Port USB-C data connection;
- confirm the selector is on `Device`;
- confirm `/dev/ttyACM0` exists.

Photo download cannot open the USB Bulk device:

- confirm `lsusb` shows the DJI device;
- rebuild after running `scripts/setup.sh`;
- run `sudo ./scripts/shoot_and_download.sh`.

PSDK 3.9.2 can emit message-queue errors during `DjiCore_DeInit()`. The CLI
avoids that defective shutdown path and lets Linux release resources at process
exit.

## Project Layout

```text
src/       C application, DJI transports, telemetry, gimbal, and camera
scripts/   setup, build, launchers, and YOLO benchmark
config/    private DJI application configuration template
```
