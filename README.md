# DJI M300 RTK on Raspberry Pi 5

Minimal application for:

- reading DJI Matrice 300 RTK telemetry;
- controlling a Zenmuse H20 gimbal;
- taking and downloading H20 photos;
- benchmarking YOLO inference on the downloaded images;
- receiving commands from an MSDK Android application through MOP.

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

## MSDK to PSDK Commands

The Raspberry Pi can expose a MOP command server to an Android MSDK
application:

```bash
sudo ./scripts/run_mop_server.sh
```

The tested configuration is:

```text
MOP channel:       49152
MSDK device type:  ONBOARD
MSDK transmission: STABLE
PSDK transmission: RELIABLE
```

Supported commands:

```text
PING
GIMBAL_YAW -10
GIMBAL_YAW 10
GIMBAL_PITCH -10
GIMBAL_PITCH 10
INFER
INFER_STATUS
```

`INFER` runs YOLO11n on the newest JPEG in `photos/`. The annotated image is
written to:

```text
inference/latest-annotated.jpg
```

The server immediately replies with `ACCEPTED INFER`, performs the inference
in a worker thread, and remains available for `INFER_STATUS` polling. The final
response contains the detection count, detected classes, inference time, total
time, input filename, and output filename.

Example:

```text
RESULT OK detections=0 classes=none inference_ms=314.5 total_ms=905.1 \
image=DJI_20260611162037_0004_Z.JPG output=latest-annotated.jpg
```

The Android test screen provides Connect, Ping, Infer, and relative gimbal
buttons. Its log automatically scrolls. Closing and reopening the Activity in
the same Android process is supported: an already registered MSDK instance is
detected through `SDKManager.isRegistered()`.

### MSDK Java Example

Initialize and connect a reliable request/response pipeline:

```java
private static final int MOP_CHANNEL_ID = 49152;

private final ExecutorService mopExecutor =
        Executors.newSingleThreadExecutor();

private IPipelineManager pipelineManager;
private volatile Pipeline pipeline;

private void connectMop() {
    pipelineManager = PipelineManager.getInstance();
    pipelineManager.init();
    pipelineManager.addPipelineConnectionListener(pipelines ->
            pipeline = pipelines.get(MOP_CHANNEL_ID));

    mopExecutor.execute(() -> {
        IDJIError error = pipelineManager.connectPipeline(
                MOP_CHANNEL_ID,
                PipelineDeviceType.ONBOARD,
                TransmissionControlType.STABLE);
        if (error != null) {
            Log.e("MOP", "Connection failed: " + error.description());
        }
    });
}
```

Use one executor for the complete write/read exchange. Do not perform a write
on one thread and its corresponding read on another thread:

```java
private String exchangeMopCommand(String command) {
    Pipeline currentPipeline = pipeline;
    if (currentPipeline == null) {
        throw new IllegalStateException("MOP is not connected");
    }

    byte[] request = (command + "\n").getBytes(StandardCharsets.UTF_8);
    DataResult writeResult = currentPipeline.writeData(request);
    if (writeResult.getError() != null) {
        throw new IllegalStateException(
                "MOP write failed: " + writeResult.getError().description());
    }

    byte[] responseBuffer = new byte[4096];
    DataResult readResult = currentPipeline.readData(responseBuffer);
    if (readResult.getError() != null) {
        throw new IllegalStateException(
                "MOP read failed: " + readResult.getError().description());
    }

    return new String(
            responseBuffer,
            0,
            readResult.getLength(),
            StandardCharsets.UTF_8).trim();
}
```

Simple asynchronous commands:

```java
mopExecutor.execute(() -> {
    String pong = exchangeMopCommand("PING");
    Log.i("MOP", "Response: " + pong);
});

mopExecutor.execute(() ->
        exchangeMopCommand("GIMBAL_PITCH -10"));
```

Start an inference without blocking the Android UI or the MSDK mission logic,
then poll until the Raspberry Pi returns the result:

```java
public void requestInference(Consumer<String> onResult) {
    mopExecutor.execute(() -> {
        String response = exchangeMopCommand("INFER");
        if (!"ACCEPTED INFER".equals(response)) {
            onResult.accept(response); // For example: BUSY INFER
            return;
        }

        for (int attempt = 0; attempt < 40; attempt++) {
            try {
                Thread.sleep(750);
            } catch (InterruptedException exception) {
                Thread.currentThread().interrupt();
                onResult.accept("RESULT ERROR interrupted");
                return;
            }

            String status = exchangeMopCommand("INFER_STATUS");
            if (status.startsWith("RESULT ")) {
                onResult.accept(status);
                return;
            }
        }

        onResult.accept("RESULT ERROR timeout");
    });
}
```

For example, a waypoint callback can enqueue the inference after the camera
reports that the photo was captured:

```java
requestInference(result ->
        runOnUiThread(() -> inferenceStatusView.setText(result)));
```

Disconnect when the owning Android component is destroyed:

```java
private void disconnectMop() {
    if (pipelineManager != null) {
        pipelineManager.disconnectPipeline(
                MOP_CHANNEL_ID,
                PipelineDeviceType.ONBOARD,
                TransmissionControlType.STABLE);
        pipelineManager.destroy();
    }
    pipeline = null;
    mopExecutor.shutdownNow();
}
```

The complete tested implementation is in
`MopTestActivity.java` in the `uav-monitor` repository.

### Concurrency Model

The MSDK application and the aircraft mission can continue operating while the
Raspberry Pi performs an inference. MOP I/O runs outside the Android UI thread,
and YOLO runs in a separate Raspberry Pi worker thread.

The current command implementation is intentionally serialized:

- Android uses one `ExecutorService` worker for all MOP request/response
  exchanges;
- the Raspberry Pi MOP loop handles one connected client and one command at a
  time;
- only one YOLO inference can be active;
- a second `INFER` received while one is running returns `BUSY INFER`;
- gimbal commands acknowledge first, then execute synchronously in the MOP
  command loop.

Therefore, sending N calls does not create N independent inference threads.
Calls from the test Activity are queued and processed in order. This avoids
concurrent access to the MOP pipeline and prevents multiple YOLO processes from
competing for Raspberry Pi memory and CPU.

For a production mission, use asynchronous job semantics:

1. MSDK sends a command containing a unique job ID and image identifier.
2. PSDK acknowledges and places the job in a bounded queue.
3. One or more explicitly configured workers process queued jobs.
4. MSDK polls job status or receives results on a dedicated result channel.

Do not add arbitrary parallel `writeData`/`readData` calls to the same MOP
pipeline. The current text protocol has no request IDs, so concurrent exchanges
could associate a response with the wrong request.

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

The Android application works on first launch but not after reopening:

- use the current `MopTestActivity`, which checks `SDKManager.isRegistered()`;
- disconnect and destroy the MOP pipeline from `Activity.onDestroy()`;
- keep the Raspberry Pi MOP server running while reopening the Activity.

PSDK 3.9.2 can emit message-queue errors during `DjiCore_DeInit()`. The CLI
avoids that defective shutdown path and lets Linux release resources at process
exit.

## Project Layout

```text
src/       C application, transports, telemetry, gimbal, camera, and MOP server
scripts/   setup, build, launchers, YOLO benchmark, and single-image inference
config/    private DJI application configuration template
```
