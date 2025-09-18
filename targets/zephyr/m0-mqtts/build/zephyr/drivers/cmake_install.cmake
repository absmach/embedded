# Install script for directory: /home/kisaka_the_jones/ULTRAVIOLET/Absmach/venvzephyr/zephyrproject/zephyr/drivers

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/home/kisaka_the_jones/zephyr-sdk-0.17.2/riscv64-zephyr-elf/bin/riscv64-zephyr-elf-objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/disk/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/firmware/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/interrupt_controller/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/misc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/pcie/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/usb/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/usb_c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/clock_control/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/console/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/entropy/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/ethernet/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/gpio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/hwinfo/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/pinctrl/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/serial/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/timer/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/kisaka_the_jones/ULTRAVIOLET/Absmach/testmqtts/embedded/targets/zephyr/m0-mqtts/build/zephyr/drivers/wifi/cmake_install.cmake")
endif()

