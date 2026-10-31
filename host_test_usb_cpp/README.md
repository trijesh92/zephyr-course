# Zephyr Board USB Vendor Host Test (C++ + WinUSB)

Host-side test application written in **C++ using the Windows WinUSB API** to communicate
with the custom USB vendor device on the Zephyr board over bulk endpoints and vendor control transfers.

## Architecture

The code is split into three cleanly separated layers, following OOP and SOLID principles:

| Layer | Interface | Concrete class | Responsibility |
|-------|-----------|----------------|----------------|
| **Transport** | `IUsbConnection` (`include/usb_connection.h`) | `WinUsbDevice` (`src/usb_connection.cpp`) | Device discovery (VID/PID), WinUSB handle management, raw bulk/control transfers, bulk endpoint auto-discovery |
| **Protocol** | `IUsbProtocol` (`include/usb_protocol.h`) | `LedUsbProtocol` (`src/usb_protocol.cpp`) | Wire-frame encoding (host→device) and decoding (device→host) |
| **Application** | `UsbHostController` (`include/usb_app.h`, `src/usb_app.cpp`) | — | Domain operations: `get_status`, `set_led` over bulk or control transfers |

The `main.cpp` entry point only assembles the stack and parses the command line — it knows nothing about WinUSB handles or wire byte layouts.

### Why this matters

- **Any WinUSB device**: `WinUsbDevice` matches VID/PID numerically by parsing the device instance path, and auto-discovers bulk IN/OUT pipes via `WinUsb_QueryPipe`. No hardcoded endpoints.
- **Pluggable protocol**: To support a different device, subclass `IUsbProtocol` (implement `encode_request`/`decode_response`) and pass it to `UsbHostController`. The transport and CLI layers remain untouched (Open-Closed Principle).
- **RAII**: All OS handles (`HANDLE`, `WINUSB_INTERFACE_HANDLE`) are owned by the connection object and released in its destructor.

## Protocol (LedUsbProtocol — the default)

**Bulk OUT** (host → device): `[opcode][arg]`
- `0x01` SET_LED, arg: `0x01` Green ON, `0x02` Green OFF, `0x03` Red ON, `0x04` Red OFF
- `0x02` GET_STATUS, arg: `0x00`

**Bulk IN** (device → host): `[status][data]`
- `0x00` OK, `0xFF` ERROR
- data: LED states (bit0=green, bit1=red)

Vendor control transfers are also supported (GET_STATUS / SET_LED as vendor requests).

## Build (Windows)

### Prerequisites

1. **Windows SDK** (includes WinUSB API headers and import libraries)
2. **CMake 3.10+** and a C++20 compiler (MinGW, MSVC, etc.)
3. **WinUSB driver** installed on the device via **Zadig**
   - Download Zadig: https://zadig.akeo.ie/
   - Plug in the board
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

## Files

| File | Purpose |
|------|---------|
| `include/usb_connection.h` | `IUsbConnection` abstract interface |
| `include/usb_protocol.h` | `IUsbProtocol` abstract interface, `UsbCommand`, `LedId`, `UsbResponse` |
| `include/usb_app.h` | `UsbHostController`, `CommandResult` |
| `src/usb_connection.cpp` | `WinUsbDevice` implementation + factory |
| `src/usb_protocol.cpp` | `LedUsbProtocol` implementation + factory |
| `src/usb_app.cpp` | `UsbHostController` implementation |
| `src/main.cpp` | CLI entry point |
| `CMakeLists.txt` | CMake build configuration (Windows SDK only) |
| `README.md` | Build and usage instructions |
