# fw_showcase

Zephyr RTOS firmware for a custom **STM32H573RI** board: a DHT11
temperature/humidity monitor with a 20x4 LCD, a built-in web dashboard and
configuration page, MQTT reporting (optionally over TLS), and an alarm that
drives an RS485 message, a relay and a motor. The motor can be switched from the
web page, over RS485, with a button, or by the alarm. Images are booted and verified by **MCUboot**
(RSA-2048 signature, downgrade prevention).

- Firmware version: **3.1.0**
- Zephyr: **v4.4.2** (pinned in [`west.yml`](west.yml))
- Board: `stm32h573ri_custom` (definition included in [`boards/`](boards))

## Features

### Sensor and display
- Reads the **DHT11** every 2 s.
- **20x4 character LCD** (HD44780 behind a PCF8574/PCF8574A I2C backpack). The
  I2C address is auto-detected (0x27/0x3F first, then 0x20-0x27 and 0x38-0x3F).
  If the LCD is unplugged, the firmware probes it again every 10 s.
- Boot screen (`fw_showcase <ver>` / `DHT11 + ALARM` / `NET WAIT`) for exactly
  5 s, then the status screen:

  ```text
  IP:192.168.1.50
  T: 28.0°C , H: 45.0%
  MQTT:OK   Motor:OFF
  !! TEMP HIGH !!          <- only while an alarm is active
  ```

  The IP is the saved static one until the Ethernet link is up and an address
  is assigned. `MQTT:OK` is shown only while the broker session is up and the
  cable is in. The IP and MQTT fields are redrawn as soon as they change; the
  sensor values every 2 s.

### Alarm
The values are checked on **every** read. Hysteresis stops the alarm from
switching on and off around a threshold.

| Condition        | Starts when             | Clears when                 |
| ---------------- | ----------------------- | --------------------------- |
| Temperature high | > 40 °C                 | <= 39 °C                    |
| Temperature low  | < 0 °C                  | >= 1 °C                     |
| Humidity high    | > 90 %                  | <= 88 %                     |
| Humidity low     | < 20 %                  | >= 22 %                     |
| Sensor fault     | 3 failed reads in a row | first good, in-range read   |

When an alarm **starts**:
- An RS485 `ALARM` message is sent at once and repeated every 10 s while the
  alarm is active.
- **Relay 1** blinks (500 ms on / 500 ms off).
- The **motor** turns on, only for **temperature high**. Humidity alarms and
  a sensor fault just blink relay 1.
- The LCD shows the warning.
- Web and MQTT are updated at once.

When it **clears**:
- An RS485 `CLEAR` message is sent.
- The relay turns off.
- The motor turns off if the temperature-high alarm started it.

Thresholds are in [`src/modules/Alarm/alarm.h`](src/modules/Alarm/alarm.h).

### Motor
Four sources can switch the motor (Relay 3, PA15):
- the **ON/OFF switch on the web page**;
- an **RS485 `ON` / `OFF`** line;
- the **PA10 button**, which toggles it once per click (press then release; polled every 10 ms, a level must hold ~50 ms);
- the **temperature-high alarm** (> 40 °C), which turns it on.

Rules:
- If the **temperature-high alarm** turned the motor on, it turns off when the
  temperature is back to normal.
- If it was switched by hand (web, RS485 or button), it is never turned off
  automatically.
- A manual OFF during an over-temperature holds until the next one.
- The web page shows who switched it last, for example `(by RS485)`.

### RS485 (send and receive)
Uses UART4 at 9600 8N1, with the direction pin on PC3 (half duplex). Messages
are ASCII lines ending in `\r\n`.

Sent by the device:

```text
ALARM #12 T=42C H=30% REASON=T_HIGH
ALARM #13 T=-- H=-- REASON=SENSOR
CLEAR #14 T=28C H=45%
ACK MOTOR ON
```

`#N` is a running message number. `REASON` is one of `T_HIGH`, `T_LOW`,
`H_HIGH`, `H_LOW` or `SENSOR`. Text typed in the RS485 box of the web page is
sent as one line.

Received by the device (case-insensitive, at most 63 characters): `ON` and
`OFF` switch the motor, the web page follows within 2 s, and the device answers
`ACK MOTOR ON` / `ACK MOTOR OFF`. A line ends with CR and/or LF, or after a
50 ms pause, so typing `ON` or `OFF` in a serial terminal is enough. Other
lines are ignored. Do not send while the device is transmitting.

### Ethernet, web and MQTT
- **W5500 Ethernet** comes up in the background, so the LCD never waits for it.
  It gets its address by DHCP: it waits for the cable without a limit, and if
  no lease arrives within 30 s after link up, it falls back to the saved
  static IP (default `192.168.1.100/24`, gateway `192.168.1.1`).
- **Web server** on port 80:
  - `/login`: password protected, one session at a time, with an increasing
    delay after each wrong password.
  - `/config` (**DASHBOARD / Device configuration**), refreshed every 2 s:
    - *Device Information*: name, firmware, MAC, IP, IP mode, web port, alarm,
      RS485.
    - *MQTT Settings*: broker, port, client ID, user, password and status.
      **Subscribe**: a topic and a button; the last 3 messages (up to 384
      characters each) are shown below (wildcards allowed, not kept across a
      reboot). **Publish**: a topic, a message and a button (max 128
      characters); the topic is sent exactly as typed, no prefix added (max 63
      characters).
    - *Devices*: `LCD 20x4 Status`, `Sensor DHT11 Status Temperature
      Humidity`, the motor ON/OFF switch, and an RS485 box with a Send button.
    - *Modify Web Login Password*.
  - `/status`: JSON.
  - **Save & Apply** stores only the MQTT settings and the password, then
    reboots. The switch, Send, Subscribe and Publish buttons act at once
    through `/api/...` (JSON, login required) and never reboot the device.
- **MQTT** (3.1.1, QoS 0):
  - Publishes the status JSON to `users/admin@example.com/<NNN>/status` every
    30 s, and at once when an alarm starts or clears or the motor is switched.
    `NNN` is the last byte of the device IP with 3 digits, for example
    `192.168.1.50` -> `users/admin@example.com/050/status`.
  - Port **8883** uses TLS. The broker certificate is checked against ISRG Root
    X1 (Let's Encrypt), with SNI. Any other port uses plain TCP.
  - Reconnects automatically if the connection drops. When the cable is
    pulled, the session is dropped at once (status `link down`) instead of
    waiting for the keepalive.

Status payload:

```json
{"id":"Board 050","fw":"3.1.0","temp":28,"humi":45,"sensor_ok":1,
 "alarm":0,"reason":"NONE","motor":1,"motor_by":"RS485","ip":"192.168.1.50"}
```

`id` is `Board NNN` (NNN as in the status topic, not the MQTT client ID setting).
`motor_by` is `web`, `RS485`, `button`, `alarm` or empty. `/status` (web, no
login) returns the same fields without `id` and `fw`, plus `cnt` (number of
DHT11 reads). Web values are updated after every DHT11 read.

### Configuration storage and boot
- The configuration is kept in **two flash copies** (`config-a` / `config-b`),
  each checked with a CRC32. If copy A is corrupted, copy B is used.
- **MCUboot**:
  - swap using scratch
  - RSA-2048 signed images
  - slot 0 validated at every boot
  - downgrade prevention
  - the running image is confirmed at boot
- **NRST swap gesture:** for 3 s after reset the LED blinks fast. Pressing
  **NRST** during that time makes the device swap to the image in the secondary
  slot (MCUboot test upgrade).
- **LED LIFE** (PA8) shows a heartbeat double blink while the firmware runs.
- **Console:** logs go to **SEGGER RTT** channel 0 over the SWD probe, so no
  UART adapter is needed.

## Hardware and pinout

### External wiring (Base Board U16 header)

| U16 pin | MCU pin | Function                                     | Notes |
| ------- | ------- | -------------------------------------------- | ----- |
| 1, 2    | +5V     | Supply for DHT11 / LCD                       | |
| 17, 18  | GND     | Common ground                                | |
| 5       | PC2     | DHT11 DATA                                   | open drain; add a 4.7k-10k pull-up to 3V3 if the module has none |
| 10      | PB6     | I2C1 SCL → LCD PCF8574 backpack              | 100 kHz |
| 9       | PB9     | I2C1 SDA → LCD PCF8574 backpack              | 100 kHz |
| 12      | PA10    | Push button, active low (internal pull-up)   | toggles the motor |

### On-board peripherals

| Function           | MCU pins                                                         |
| ------------------ | ---------------------------------------------------------------- |
| W5500 Ethernet     | SPI1: SCK PA5, MISO PA6, MOSI PA7, CS PA4; INT PC4, RESET PC5     |
| RS485              | UART4: TX PA0, RX PA1; DE/RE (direction) PC3                     |
| Relay 1            | PB5 (active high), blinks on any alarm                           |
| Relay 3            | PA15 (active high), switches the motor via CN3-5 COM3 / CN3-6 OUT3 |
| LED LIFE           | PA8 (active low)                                                 |
| SWD / RTT console  | PA13 SWDIO, PA14 SWCLK                                           |

Peripherals on the board that this firmware disables:
- SHT41 / I2C2
- CAN (FDCAN1)
- SD card (SDMMC1)
- USART1, USART2, USART6
- SPI2

See [`boards/stm32h573ri_custom.overlay`](boards/stm32h573ri_custom.overlay).

### Flash layout (2 MB)

| Partition     | Offset     | Size    |
| ------------- | ---------- | ------- |
| mcuboot       | 0x000000   | 128 KB  |
| image-0       | 0x020000   | 704 KB  |
| image-scratch | 0x0D0000   | 128 KB  |
| image-1       | 0x0F0000   | 704 KB  |
| config-a      | 0x1A0000   | 32 KB   |
| config-b      | 0x1A8000   | 32 KB   |
| swapmark      | 0x1B0000   | 8 KB    |

## Build

### 1. Prerequisites (install once)
- Linux (tested on Ubuntu) with `git`, `cmake` >= 3.20, `ninja`, `python3` and
  `python3-venv`:
  ```sh
  sudo apt install git cmake ninja-build python3 python3-venv
  ```
- For flashing: [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)
  with `STM32_Programmer_CLI` on `PATH`, and an ST-LINK probe.

The Zephyr SDK does not need to be installed by hand. `setup.sh` installs it.

### 2. Clone and set up

Clone into an **empty directory**. Zephyr and the modules are downloaded next
to the clone.

```sh
mkdir fw_showcase-workspace && cd fw_showcase-workspace
git clone https://github.com/dmi-tech/fptUniversity.git
./fptUniversity/fw_showcase/scripts/setup.sh
```

`setup.sh` does the following. It is safe to run again, for example after
`west.yml` changes.
1. Checks the host tools.
2. Creates `.venv` and installs west.
3. Runs `west init` and `west update`, which fetches Zephyr v4.4.2 and the
   modules.
4. Installs the Python requirements of Zephyr and MCUboot.
5. Installs the Zephyr SDK with the `arm-zephyr-eabi` toolchain, using the
   version in `zephyr/SDK_VERSION`. This is skipped if that version is already
   installed.
6. Warns if `STM32_Programmer_CLI` or the signing key is missing.

The resulting layout:

```text
fw_showcase-workspace/
├── .venv/  .west/
├── fptUniversity/    <- git repository (github.com/dmi-tech/fptUniversity)
│   └── fw_showcase/  <- this project
├── zephyr/           <- Zephyr v4.4.2
├── bootloader/mcuboot/
└── modules/          <- hal_stm32, mbedtls, segger, ...
```

### 3. Signing key
MCUboot only boots images signed with its key, and no key is stored in the
repository. The scripts read the key from `STM32H573_MCUBOOT_KEY_FILE`.

- **Existing project key** (ask the maintainer): the devices in the field
  accept only images signed with this key.
  ```sh
  export STM32H573_MCUBOOT_KEY_FILE=~/keys/fw_showcase-rsa2048.pem
  ```
- **New key**, only for your own boards. Keep it out of the repository and back
  it up:
  ```sh
  .venv/bin/python bootloader/mcuboot/scripts/imgtool.py keygen -t rsa-2048 -k ~/keys/fw_showcase-rsa2048.pem
  export STM32H573_MCUBOOT_KEY_FILE=~/keys/fw_showcase-rsa2048.pem
  ```

Add the `export` line to `~/.bashrc` so every new terminal has it.

### 4. Build

```sh
./fptUniversity/fw_showcase/scripts/build.sh            # output: fptUniversity/fw_showcase/build
./fptUniversity/fw_showcase/scripts/build.sh /tmp/out   # or a custom build directory
```

The scripts use the workspace `.venv` themselves, so you do not need to
activate it. `build.sh` runs a sysbuild build of MCUboot and the application,
and signs the application with your key. Output files:

| File                                   | Contents                       |
| -------------------------------------- | ------------------------------ |
| `build/mcuboot/zephyr/zephyr.hex`      | MCUboot bootloader             |
| `build/fw_showcase/zephyr/zephyr.signed.hex` / `.bin` | Signed application |

## Flash

Connect the ST-LINK to SWD and run:

```sh
./fptUniversity/fw_showcase/scripts/flash.sh            # build + flash MCUboot and the app
```

Or flash an existing build with west. Activate the venv first:

```sh
source .venv/bin/activate
west flash -d fptUniversity/fw_showcase/build            # ST-LINK (STM32CubeProgrammer)
west flash -d fptUniversity/fw_showcase/build -r jlink   # J-Link
```

To update the application only, flash `zephyr.signed.hex` to image-0, or place
a signed image in image-1 and use the NRST swap gesture.

## Console (RTT)

Logs are on SEGGER RTT channel 0. You can read them with either:
- **J-Link:** `JLinkRTTViewer`, or `JLinkRTTLogger -Device STM32H573RI -If SWD -Speed 4000 -RTTChannel 0 log.txt`
- **ST-LINK + OpenOCD:** `rtt setup 0x20000000 0x40000 "SEGGER RTT"`, then
  `rtt start` and `rtt server start 19021 0`, and connect to TCP port 19021.

## First use

1. Plug in Ethernet and power the board. The LCD shows the IP address.
2. Open `http://<device-ip>/` and log in with the default password **`123456`**.
3. Fill in the **MQTT Settings**: broker host, port (`8883` for TLS), client
   ID, user and password. They are empty on a new device, and MQTT stays
   `not configured` until they are set. The status topic needs no setting.
4. **Change the web password**, then click **Save & Apply**. The device reboots
   with the new settings.

## Project structure

```text
fptUniversity/fw_showcase/
├── west.yml                 west manifest (Zephyr v4.4.2 + needed modules)
├── CMakeLists.txt
├── prj.conf                 application Kconfig
├── sysbuild.conf            MCUboot selection / signature type
├── sysbuild/mcuboot.conf    MCUboot Kconfig
├── boards/
│   ├── st/stm32h573ri_custom/       board definition (DTS, pinctrl, Kconfig)
│   └── stm32h573ri_custom.overlay   showcase-specific devicetree changes
├── scripts/
│   ├── setup.sh             one-time workspace setup (west, Python, SDK)
│   ├── build.sh             build (never flashes)
│   └── flash.sh             build + flash over ST-LINK
└── src/
    ├── main.c               start-up sequence and main loop
    └── modules/             one folder per module
        ├── Alarm/
        │   ├── alarm.c/.h         thresholds, relay blink
        │   └── alarm_notify.c/.h  RS485 ALARM/CLEAR messages
        ├── Boot/
        │   └── boot_swap.c/.h     image confirm, NRST swap gesture
        ├── Config/
        │   ├── app_config.c/.h    configuration struct, flash A/B storage
        │   ├── app_state.c/.h     state shared by main loop, web and MQTT
        │   └── version.h
        ├── DHT11/
        │   └── dht11.c/.h         DHT11 (Zephyr aosong,dht driver)
        ├── LCD/
        │   ├── lcd_pcf8574.c/.h   HD44780 20x4 over PCF8574
        │   └── lcd_view.c/.h      LCD screen layout
        ├── Motor/
        │   └── motor.c/.h         motor output, PA10 button, who may switch it
        ├── MQTT/
        │   ├── mqtt_app.c/.h      MQTT client thread
        │   ├── mqtt_ca.h          TLS trust anchor (ISRG Root X1)
        │   └── TLS/
        │       └── mbedtls_user_config.h   TLS buffer sizes
        ├── Network/
        │   └── network.c/.h       Ethernet up, DHCP / static IP
        ├── RS485/
        │   ├── rs485.c/.h         UART4 + direction pin, line receiver
        │   └── rs485_cmd.c        RS485 ON/OFF command thread
        ├── StatusLED/
        │   └── status_led.c/.h    LED LIFE heartbeat
        └── WebServer/
            ├── http.c/.h          HTTP helpers
            ├── web_server.c       routes, login, config page, /api tools
            └── web_pages.h        HTML / CSS / JS
```

The web server, the MQTT client and the RS485 command handler each run in
their own thread (`K_THREAD_DEFINE`). The main loop reads the sensor, runs the
alarm, updates the LCD and hands each new reading to web and MQTT through
`app_state`. Only the MQTT thread calls the MQTT library; the web thread queues
its Subscribe / Publish requests.

## License

[Apache-2.0](LICENSE)
