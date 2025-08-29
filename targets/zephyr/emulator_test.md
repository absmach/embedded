# Zephyr RTOS - Hello World on QEMU RISC-V

This README educates on building and running the **Zephyr RTOS Hello World** sample application using a **Python virtual environment** and the **West build system**.  
It targets the `qemu_riscv32` board for testing in a simulated environment.

---

## 📦 Requirements

Before starting, ensure you have the following installed on your system:

- **Python 3.8+** (recommended 3.10+)
- **pip** and **venv**
- **CMake** (>= 3.20)
- **Ninja Build System**
- **dtc** (Device Tree Compiler)
- **Zephyr dependencies** (`west`, compilers, etc.)
- **QEMU** (for simulation)

1. On Ubuntu/Debian, you can install most dependencies with:

```bash
sudo apt update
sudo apt install --no-install-recommends \
    git cmake ninja-build gperf \
    ccache dfu-util device-tree-compiler wget \
    python3-pip python3-venv python3-dev \
    qemu-system-misc
```

2. Create and activate a Python virtual environment

```bash
python3 -m venv my-venv
source my-venv/bin/activate
```

You should see (my-venv) at the beginning of your terminal prompt once activated.

3. Install West (Zephyr’s meta-tool)

```bash
pip install --upgrade pip
pip install west
```

4. Initialize the Zephyr project

```bash
west init zephyrproject
cd zephyrproject
west update
west zephyr-export
```

5. Install Zephyr dependencies

```bash
pip install -r zephyr/scripts/requirements.txt
```

6. Building the Hello World sample

Navigate to the Hello World sample directory.

```bash
cd zephyr/samples/hello_world
```

7. Build the sample for the qemu_riscv32 target.

```bash
west build -b qemu_riscv32 -s . -d build
```

8. After a successful build, run the application inside QEMU.

```bash
west build -t run
```

9. If successful, you should see:

```bash
-- west build: running target run
[0/1] To exit from QEMU enter: 'CTRL+a, x'[QEMU] CPU: riscv32
*** Booting Zephyr OS build v4.2.0-415-g1f69b91e909c ***
Hello World! qemu_riscv32/qemu_virt_riscv32
```
