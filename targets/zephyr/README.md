# Zephyr

This folder contains the Zephyr target configurations for the Absmach project.

## How to use

To use the Zephyr target configurations, you need to have Zephyr installed on your system. You can find the installation instructions [here](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).

Once Zephyr is installed, you can run the following commands on `targets/zephyr` directory:

```bash
source ~/zephyrproject/.venv/bin/activate
west update
```

Currently, we are using Zephyr version 4.2.0.

You can then build and flash the project to your board using the following commands:

```bash
west build -b <board-name> <project-name>
west flash
```

For example, to build and flash the project to the ESP32 DevKitC WROOM board, you can run the following commands:

```bash
west build -p always -b esp32_devkitc_wroom/esp32/procpu hello_world
west flash
west espressif monitor
```

## Supported Boards

The following boards are supported by the Zephyr target configurations:

- ESP32 DevKitC WROOM <esp32_devkitc_wroom>
