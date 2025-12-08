# ESP32-C6 Debug Example

This is a simple example of a debug application for the ESP32-C6 board.

## Build

To build the application, you need to install the ESP-IDF framework and the toolchain.

1. Install the ESP-IDF framework:

    ```bash
    git clone --recursive https://github.com/espressif/esp-idf.git
    cd esp-idf
    ./install.sh
    ```

2. Install the toolchain:

    ```bash
    cd ~/esp
    git clone --recursive https://github.com/espressif/esp-idf-tools.git
    cd esp-idf-tools
    ./install.sh
    ```

3. Build the application:

    ```bash
    cd targets/esp-idf/debug
    idf.py set-target esp32c6
    idf.py build
    ```

4. Flash and monitor the application:

    ```bash
    idf.py -p /dev/ttyACM0 flash monitor
    ```

5. Press `F5` to start debugging the application.

