# Zephyr Board USB Vendor Host Test (C++ + WinUSB)

Host-side test application written in **C++ using the Windows WinUSB API** to communicate
with the custom USB vendor device on the Zephyr board over bulk endpoints.

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

Vendor control transfers are also supported (vendor-specific control requests).

## Build (Windows)

### Prerequisites

1. **Windows SDK** (includes WinUSB API headers and libraries)
2. **CMake** and a C++ compiler (MinGW, MSVC, etc.)
3. **WinUSB driver** installed on the device via **Zadig**
   - Download Zadig: https://zadig.akeo.ie/
   - Plug in the FRDM board (VID=0x1234, PID=0x5678)
   - Run Zadig → Options → List All Devices
   - Select your device, choose **WinUSB** driver, click **Install Driver**

### Build steps (MinGW)

```powershell
mkdir zephyr-course/host_test_usb_cpp/build
cd zephyr-course/host_test_usb_cpp/build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

### Build steps (MSVC / Visual Studio)

```powershell
mkdir zephyr-course/host_test_usb_cpp/build
cd zephyr-course/host_test_usb_cpp/build
cmake ..
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
| `Device not found` | Make sure the board is connected, powered on, and the firmware is running. Install WinUSB driver via Zadig. |
| `Access denied` | Make sure no other program has the device open. Reinstall driver via Zadig. |
| `Pipe error` | The device may be busy. Try again or reset the board. |
| `WinUsb_Initialize failed` | The WinUSB driver may not be properly installed. Reinstall via Zadig. |

## Files

| File | Purpose |
|------|---------|
| `main.cpp` | CLI application using WinUSB API (SetupAPI + WinUsb.dll) |
| `usb_protocol.h` | Protocol constants (VID/PID, endpoints, opcodes, frame sizes, GUIDs) |
| `CMakeLists.txt` | CMake build configuration (Windows SDK only) |
| `README.md` | Build and usage instructions |
