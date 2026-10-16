# SmartLighting

Showcase firmware for a **dusk-to-dawn lamp** on the Base Board STM32H573RI.
A **BH1750** light sensor measures the light and the lamp switches **on when
it gets dark and off when it gets bright**. It can also be switched by hand,
has an **off timer**, shows everything on a **20x4 LCD**, is controlled from a
**web page** and reports over **MQTT**.

Firmware version: **1.0.0** ([`VERSION`](VERSION)) · Step-by-step guide
(Vietnamese): [`docs/guideline.md`](docs/guideline.md)

## Features

- **AUTO**: the lamp turns on when the light stays below 10 lx for 30 s and
  off when it stays above 30 lx for 30 s, so it does not flicker at dusk or
  react to shadows and headlights. The lamp's own light on the sensor is
  taken into account.
- **MANUAL**: BTN1 or the web page switches the lamp; BTN2 or the web page
  goes back to AUTO.
- **Off timer** 0..12 h (BTN3 / BTN4 or the web page): the lamp switches off
  after that time; in AUTO it waits for the next night.
- **Settings** (thresholds, confirm time, timer) changed on the web page and
  kept after a power cut.
- **LCD**: IP, light, Dark / Bright, lamp, mode, MQTT, time left on the timer;
  a warning screen if the sensor fails.
- **Web page** (password protected): live values, lamp control, timer,
  settings, MQTT settings.
- **MQTT**: full status every 30 s and a message at once when something
  changes (`users/admin@example.com/<NNN>/status`, `.../lamp`, `.../light`,
  ...). TLS on port 8883.
- Secure boot with **MCUboot** (signed firmware).

## Hardware

| Device | Board pin |
| ------ | --------- |
| BH1750 module (GY-30 / GY-302) | U16-9 SDA, U16-10 SCL, 5 V, GND (shared with the LCD) |
| LCD 20x4 with I2C module | U16-9 SDA, U16-10 SCL, 5 V, GND |
| Lamp 12/24 V | Relay 3: CN3-5 COM3 / CN3-6 OUT3 |
| BTN1 lamp · BTN2 AUTO/MANUAL · BTN3 timer +1 h · BTN4 timer -1 h | U16-12, U16-11, U16-3, U16-4 (to GND) |
| Ethernet, power | RJ45, 24 V DC |

Place the sensor where the lamp does not shine on it directly. Relay 3 is for
low voltage lamps (<= 24 V DC) only.

## Setup

On Ubuntu, with `git`, `cmake`, `ninja-build`, `python3-venv`,
[STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)
and an ST-LINK on J3:

```sh
mkdir showcase-workspace && cd showcase-workspace
git clone --filter=blob:none --sparse https://github.com/dmi-tech/fptUniversity.git
git -C fptUniversity sparse-checkout set showcase/SmartLighting   # only this showcase
./fptUniversity/showcase/SmartLighting/scripts/setup.sh       # once: Zephyr, SDK

export STM32H573_MCUBOOT_KEY_FILE=~/keys/showcase-rsa2048.pem # MCUboot signing key
./fptUniversity/showcase/SmartLighting/scripts/flash.sh       # build + flash
```

The clone downloads only this showcase; keep its folder name `fptUniversity`
and use git (not a zip download), the scripts need it. `scripts/build.sh`
only builds. No signing key is stored in the repository;
see the guideline to get or create one.

## First use

1. Plug in Ethernet and power the board; the LCD shows the IP address.
2. Open `http://<IP>/` and log in with **`123456`**.
3. Fill in the MQTT broker, change the password, **Save & Apply**.

## License

[Apache-2.0](LICENSE)
