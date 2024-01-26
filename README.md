# Description

The **DMM** is the master board of the DINFox project. It embeds the following features:

* **RS485** communication to monitor and control slaves on the bus.
* **HMI** for local monitoring and control.
* Analog **measurements** such as USB, RS485 bus, and HMI voltages.

# Hardware

The board was designed on **Circuit Maker V2.0**. Below is the list of hardware revisions:

| Hardware revision | Description | Status |
|:---:|:---:|:---:|
| [DMM HW1.0](https://365.altium.com/files/ED83B6F3-90FC-4C58-A588-77DC635C6A63) | Initial version. | :white_check_mark: |

# Embedded software

## Environment

The embedded software is developed under **Eclipse IDE** version 2023-09 (4.29.0) and **GNU MCU** plugin. The `script` folder contains Eclipse run/debug configuration files and **JLink** scripts to flash the MCU.

## Target

The board is based on the **STM32L081CBT6** microcontroller of the STMicroelectronics L0 family. Each hardware revision has a corresponding **build configuration** in the Eclipse project, which sets up the code for the selected board version.

## Structure

The project is organized as follow:

* `inc` and `src`: **source code** split in 6 layers:
    * `registers`: MCU **registers** adress definition.
    * `peripherals`: internal MCU **peripherals** drivers.
    * `utils`: **utility** functions.
    * `components`: external **components** drivers.
    * `nodes`: **Node interfaces** layer.
    * `applicative`: high-level **application** layers.
* `startup`: MCU **startup** code (from ARM).
* `linker`: MCU **linker** script (from ARM).

## Dependencies

The `inc/dinfox` and `src/dinfox` folders of the [XM project](https://github.com/Ludovic-Lesur/xm) must linked to the DMM project, as they contain common data definition related to the DINFox system.
