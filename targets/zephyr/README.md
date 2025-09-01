# Zephyr

This folder contains the Zephyr target configurations for the [S0](https://absmach.github.io/s0-docs/), IoT Gateway for Wireless and Wired M-Bus Metering.

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
- ESP32 S3 <esp32s3_devkitc/esp32s3/procpu>

## Testing using Emulators (QEMU and NATIVESIM)

## Using QEMU

Build the sample for the qemu_riscv32 target.

```bash
west build -b qemu_riscv32 -s . -d build
```

After a successful build, run the application inside QEMU.

```bash
west build -t run
```

If successful, you should see:

```bash
-- west build: running target run
[0/1] To exit from QEMU enter: 'CTRL+a, x'[QEMU] CPU: riscv32
*** Booting Zephyr OS build v4.2.0-415-g1f69b91e909c ***
Hello World! qemu_riscv32/qemu_virt_riscv32
```

## Using NATIVE_SIM

While in the project directory,

Build the sample for the native_sim target.

```bash
west build -b native_sim -s . -d build
```

After building, Zephyr creates a Linux executable, which is in the build directory. To run it:

```bash
./build/zephyr/zephyr.exe
```

If you want to rerun the code after modifying it:

```bash
west build -t run
```
