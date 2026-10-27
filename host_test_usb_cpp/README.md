# Zephyr Board USB Vendor Host Test (C++)

Host-side test application written in C++ using **libusb-1.0** to communicate
with the custom USB vendor device on the Zephyr board.

## Protocol

The device exposes:
- **Bulk OUT endpoint 0x01** — host sends commands
- **Bulk IN endpoint 0x81** — device sends responses

**Command frame** (bulk OUT): `[opcode][arg]`
- `0x01` SET_LED, arg: `0x01` Green ON, `0x02` Green OFF, `0x03` Red ON, `0x04` Red OFF
- `0x02` GET_STATUS, arg: `0x00`

**Response frame** (bulk IN): `[status][data]`
- `0x00` OK, `0xFF` ERROR
- data: LED states (bit0=green, bit1=red)

Vendor control transfers are also supported.

## Build (Windows)

### Prerequisites

1. **libusb-1.0 development package** installed
   - Download from: https://github.com/libusb/libusb/releases
   - Or install via vcpkg: `vcpkg install libusb`
   - Or install via MSYS2: `pacman -S mingw-w64-x86_64-libusb`

2. **CMake** and a C++ compiler (MinGW, MSVC, etc.)

3. **USB driver** installed on the device via **Zadig**
   - Download Zadig: https://zadig.akeo.ie/
   - Plug in the FRDM board (VID=0x1234, PID=0x5678)
   - Run Zadig → Options → List All Devices
   - Select your device, choose **WinUSB** or **libusb-win32**, click Install Driver

### Build steps

```powershell
# Create build directory
mkdir zephyr-course/host_test_usb_cpp/build
cd zephyr-course/host_test_usb_cpp/build

# Configure (example for MinGW Makefiles)
cmake -G "MinGW Makefiles" ..

# Build
cmake --build .
```

The executable `host_test_usb.exe` will be in the build directory.

## Usage

```powershell
# Query LED status via bulk endpoint
host_test_usb.exe --vid 0x1234 --pid 0x5678 status

# Turn Green LED on via bulk endpoint
host_test_usb.exe --vid 0x1234 --pid 0x5678 led-on G

# Turn Green LED off via bulk endpoint
host_test_usb.exe --vid 0x1234 --pid 0x5678 led-off G

# Turn Red LED on via bulk endpoint
host_test_usb.exe --vid 0x1234 --pid 0x5678 led-on R

# Turn Red LED off via bulk endpoint
host_test_usb.exe --vid 0x1234 --pid 0x5678 led-off R

# Query LED status via control transfer
host_test_usb.exe --vid 0x1234 --pid 0x5678 ctrl-status

# Turn Green LED on via control transfer
host_test_usb.exe --vid 0x1234 --pid 0x5678 ctrl-led-on G

# Custom VID/PID and timeout
host_test_usb.exe --vid 0x1234 --pid 0x5678 --timeout 3000 status
```

## Exit codes

- `0` — success
- `1` — error (device not found, transfer failed, invalid command, etc.)

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `libusb-1.0.dll not found` | Place `libusb-1.0.dll` next to `host_test_usb.exe` or add it to PATH |
| `Device not found` | Make sure the board is connected, powered on, and the firmware is running. Install WinUSB/libusb driver via Zadig. |
| `Access denied` | Make sure no other program has the device open. Reinstall driver via Zadig. |
| `Pipe error` | The device may be busy. Try again or reset the board. |
