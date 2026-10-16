# showcase

Showcase firmwares for the **Base Board STM32H573RI** (`stm32h573ri_custom`),
Zephyr RTOS v4.4.2 + MCUboot.

Each folder is a complete, independent project; use one without the others.

| Showcase | What it does |
| -------- | ------------ |
| [SmartEnvironment](SmartEnvironment) | greenhouse monitor: temperature / humidity warnings, automatic mist watering, spray servo |
| [SmartLighting](SmartLighting) | dusk-to-dawn lamp with manual control and off timer |

Each one has an LCD, a web page and MQTT, and a step-by-step guide in
`docs/guideline.md` (Vietnamese).

Quick start (see the README of the showcase):

```sh
mkdir showcase-workspace && cd showcase-workspace
git clone --filter=blob:none --sparse https://github.com/dmi-tech/fptUniversity.git
git -C fptUniversity sparse-checkout set showcase/SmartEnvironment   # one showcase only
./fptUniversity/showcase/SmartEnvironment/scripts/setup.sh
export STM32H573_MCUBOOT_KEY_FILE=~/keys/showcase-rsa2048.pem
./fptUniversity/showcase/SmartEnvironment/scripts/flash.sh
```
