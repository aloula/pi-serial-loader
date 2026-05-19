# pi-serial-loader

A second-stage bootloader for Raspberry Pi that loads kernels over UART.

This project is based on the original PiLoader by Jurgis Brigmanis (Velko):

https://github.com/Velko/PiLoader

It keeps the original protocol and adds support and reliability improvements for newer boards.

## What it does

- Loads plain binaries and ELF executables over serial.
- Preserves the firmware handoff registers `r0-r2`.
- Relocates itself out of the load area before processing commands.
- Supports monitor mode after `EXEC`.
- Supports watchdog control and reboot on request.
- Optimizes binary uploads by sending large zero-filled ranges as `ZERO` commands.

## Improvements in this fork

- Raspberry Pi 3/3B+ support via `--enable-rpi3`.
- Updated PL011 UART setup for Pi 2/3 clocking.
- Safer client/server handshake with timeout-aware reads, resend on PING, and baud probing.
- Runtime baud switching on the client.
- Cleaner handoff before `EXEC`.
- Modernized build guidance for `arm-none-eabi` toolchains and `autoreconf -fiv`.

## Build

### Bootloader

```bash
cd raspi
autoreconf -fiv
./configure --host=arm-none-eabi --enable-rpi3
make
```

Copy the resulting `kernel.img` or `kernel7.img` to the Raspberry Pi SD card.

For Raspberry Pi 3/3B+, use the following `config.txt` settings if UART output is not routed to GPIO14/GPIO15:

```ini
arm_64bit=0
enable_uart=1
initial_turbo=0
disable_splash=1
dtoverlay=pi3-disable-bt
kernel=kernel7.img
```

### Client

```bash
cd client
autoreconf -fiv
./configure
make
sudo make install
```

Run a kernel with:

```bash
./piboot -p /dev/ttyUSB0 ../samplekernel/kernel.elf
```

Useful options:

- `-m` starts the serial monitor after `EXEC` (read-only).
- `-t` starts a full two-way serial terminal after `EXEC`. Use `Ctrl-X` to exit.
- `-a` automatically reloads the kernel when the Pi is power-cycled (waits for "Waiting connection...").
- `-w` disables the watchdog for kernels that manage it themselves.
- `-v` enables verbose output.
- `-c | --config <file>` loads configuration from a file.

## Configuration System

You can simplify your command line by using a configuration file. `piboot` automatically looks for `piboot.conf` in the current directory and `~/.pibootrc` in your home folder.

Example `piboot.conf`:
```ini
port = /dev/ttyUSB0
terminal = true
verbose = false
auto_load = true
no_watchdog = true
```

Command-line arguments will always override settings in the configuration file.

## Recent Improvements

- **Interactive Terminal Mode**: Added `-t` / `--terminal` to allow interacting with the loaded kernel via UART. Includes raw mode support for arrow keys and terminal control characters.
- **Robust Error Handling**: Added strict verification for all file I/O and serial communications, eliminating silent failures and compiler warnings.
- **Improved Monitoring**: The monitor and terminal modes now use `select()` for reliable multiplexing and provide clear status messages.
- **Reliable Exit**: Terminal mode uses `Ctrl-X` for a clean exit, restoring your local terminal settings and allowing for graceful termination even in auto-reload mode.

## Using with WSL (Windows Subsystem for Linux)

To use `piboot` within WSL, you need to pass through the serial port from the Windows host using `usbipd-win`.

### Prerequisites

1.  Install [usbipd-win](https://github.com/dorssel/usbipd-win) on Windows.
2.  Install the `usbip` client and hardware database in your WSL distribution:
    ```bash
    sudo apt update
    sudo apt install linux-tools-generic hwdata
    sudo update-alternatives --install /usr/local/bin/usbip usbip /usr/lib/linux-tools/*-generic/usbip 20
    ```

### Binding and Attaching

1.  Open a Windows Terminal with **Administrator** privileges.
2.  List available USB devices:
    ```powershell
    usbipd list
    ```
3.  Identify your serial adapter (e.g., "USB-Serial CH340") and its BUSID (e.g., `2-1`).
4.  Bind the device (only needed once per device):
    ```powershell
    usbipd bind --busid <BUSID>
    ```
5.  Attach the device to WSL:
    ```powershell
    usbipd attach --wsl --busid <BUSID>
    ```
6.  The device should now be visible in WSL as `/dev/ttyUSB0` (or similar). You can verify with `ls /dev/ttyUSB*`.

When finished, you can detach it from Windows:
```powershell
usbipd detach --busid <BUSID>
```

### Sample kernel

```bash
cd samplekernel
autoreconf -fiv
./configure --host=arm-none-eabi --enable-rpi3
make
```

## Tested examples

- `../samplekernel/kernel.elf` with `-v -w -m` (prints `Hello, World!`).

Both were verified successfully with `LOAD`, `ZERO`, and `EXEC` completing on the bootloader.

## Troubleshooting

- No kernel output after `EXEC ... OK`: run with `-m` so `piboot` stays in monitor mode.
- Garbled monitor output: ensure host and target UART are both at 115200.
- Sample kernel on Pi 2/3/3B+: build with `--enable-rpi3`; the sample UART setup in this fork uses Pi 2/3 PL011 divisors (`IBRD=26`, `FBRD=3`) for 115200.
- If you changed client or samplekernel sources, rebuild before retesting:

```bash
cd client && make
cd ../samplekernel && make
```

## Notes

The project is intentionally small and direct. If you need the original longer background and rationale, see the upstream PiLoader repository linked above.

