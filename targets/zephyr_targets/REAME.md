# Zephyr Targets

This folder contains the Zephyr target configurations for the Absmach project.

## How to use

To use the Zephyr target configurations, you need to have Zephyr installed on your system. You can find the installation instructions [here](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).

Once Zephyr is installed, you can run the following commands on `targets/zephyr_targets` directory:

```bash
west init
west update
```

You can then build and flash the project to your board using the following commands:

```bash
west build -b <board-name> <project-name>
west flash
```

For example, to build and flash the project to the ESP32 DevKitC WROOM board, you can run the following commands:

```bash
west build -b esp32_devkitc_wroom/esp32/procpu coap
west flash
```

## Supported Boards

The following boards are supported by the Zephyr target configurations:

- ESP32 DevKitC WROOM <esp32_devkitc_wroom>

## Customizing the Target Configurations

If you want to customize the target configurations, you can do so by modifying the `absmach.conf` file in this folder. This file contains the configuration for the Absmach board, and you can modify it to suit your needs.

For example, if you want to change the default flash size, you can modify the `CONFIG_FLASH_SIZE` variable in the `absmach.conf` file. Here's an example of how you can change the flash size to 128 MB:

```ini
CONFIG_FLASH_SIZE=128
```

After making the changes, you can rebuild the project and flash it to the Absmach board using the following commands:

```bash
cd <project-directory>
west build -b absmach
west flash
```

Note that you need to have the Absmach board connected to your computer for the flashing process to work.
