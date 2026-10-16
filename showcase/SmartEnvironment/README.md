# SmartEnvironment

Showcase firmware for a small greenhouse / garden with mist watering on the
Base Board STM32H573RI. A **DHT11** measures temperature and humidity, the
board warns when the environment is bad for the plants, **waters
automatically** with a mini pump and points the spray with a **servo**.
Everything is shown on a **20x4 LCD**, controlled from a **web page** and
reported over **MQTT**.

Firmware version: **1.0.0** ([`VERSION`](VERSION)) · Step-by-step guide
(Vietnamese): [`docs/guideline.md`](docs/guideline.md)

## Features

- **Environment check** every 2 s: hot / cold, dry / humid, extreme heat and
  sensor fault, each with a level (OK, warning, critical) and an advice.
  Relay 1 blinks slowly on a warning, fast on a critical situation.
- **Automatic watering** when the air is hot and dry or too dry: the pump runs
  in short cycles (20 s on, 60 s off, at most 10 per hour).
- **Manual pump**: BTN1 or the web page. Switching it off during automatic
  watering pauses the automatic mode until **Re-enable auto** on the web page.
- **Spray servo**: AUTO sweeps while the pump runs; MANUAL is turned with
  BTN3 / BTN4 or set on the web page. Hold BTN2 for 3 s to switch the mode.
- **LCD**: IP, temperature, humidity, pump, servo mode, MQTT; a warning screen
  with the reason while the environment is not OK.
- **Web page** (password protected): live values, advice, pump and servo
  control, MQTT settings.
- **MQTT**: full status every 30 s and a message at once when a device
  changes (`users/admin@example.com/<NNN>/status`, `.../pump`, `.../env`, ...).
  TLS on port 8883.
- Secure boot with **MCUboot** (signed firmware).

## Hardware

| Device | Board pin |
| ------ | --------- |
| DHT11 | U16-5 (PC2), 5 V, GND |
| LCD 20x4 with I2C module | U16-9 SDA, U16-10 SCL, 5 V, GND |
| Servo signal | U16-16 (PB14); servo on its own 5 V supply, common GND |
| Mini pump | Relay 3: CN3-5 COM3 / CN3-6 OUT3 |
| Alert lamp / buzzer (optional) | Relay 1: CN3-1 COM1 / CN3-2 OUT1 |
| BTN1 pump · BTN2 servo mode · BTN3 left · BTN4 right | U16-12, U16-11, U16-3, U16-4 (to GND) |
| Ethernet, power | RJ45, 24 V DC |

## Setup

On Ubuntu, with `git`, `cmake`, `ninja-build`, `python3-venv`,
[STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)
and an ST-LINK on J3:

```sh
mkdir showcase-workspace && cd showcase-workspace
git clone --filter=blob:none --sparse https://github.com/dmi-tech/fptUniversity.git
git -C fptUniversity sparse-checkout set showcase/SmartEnvironment   # only this showcase
./fptUniversity/showcase/SmartEnvironment/scripts/setup.sh    # once: Zephyr, SDK

export STM32H573_MCUBOOT_KEY_FILE=~/keys/showcase-rsa2048.pem # MCUboot signing key
./fptUniversity/showcase/SmartEnvironment/scripts/flash.sh    # build + flash
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
