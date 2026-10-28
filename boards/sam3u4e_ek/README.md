# Atmel SAM3U4E-EK board support for Zephyr

Summary of the changes required to add a new board based on the **AT91SAM3U4E**
microcontroller and to make the Zephyr `samples/basic/blinky` example build and
run on it.

## Files added / modified

### Out-of-tree board — `zephyr-course/boards/sam3u4e_ek/`

| File | Purpose |
|------|---------|
| `board.yml` | Board metadata: name `sam3u4e_ek`, vendor `atmel`, soc `sam3u4e` |
| `board.cmake` | J-Link runner configuration (`--device=AT91SAM3U4E`, `--speed=4000`) |
| `CMakeLists.txt` | Board library sourcing `board.c` |
| `board.c` | Minimal board initialization (`SYS_INIT`) |
| `Kconfig.defconfig` | GPIO enabled, console disabled |
| `Kconfig.sam3u4e_ek` | `config BOARD_SAM3U4E_EK` selecting `SOC_SAM3U4E` |
| `sam3u4e_ek_defconfig` | Fixed config: `GPIO=y`, `FLASH_SIZE=256`, `SRAM_SIZE=48`, base addresses |
| `sam3u4e_ek.dts` | Devicetree: `led0` on **PIOA pin 0** (`GPIO_ACTIVE_HIGH`), chosen `zephyr/sram` & `zephyr/flash`, 28 MHz CPU clock |
| `README.md` | This summary |

> **Board build (verified):**
> ```
> west build -b sam3u4e_ek samples/basic/blinky \
>   -- -DBOARD_ROOT=<path>/zephyr-course
> ```
> Result: `zephyr.elf` links — FLASH 11304 B / 256 KB (4.3%), RAM 3832 B / 48 KB (7.8%).
> ELF confirmed 32-bit little-endian ARM (EM_ARM), entry `0xA95` (Reset_Handler).

### In-tree SoC port — `deps/zephyr/soc/atmel/sam/sam3u/` (new)

Mirrors the existing SAM3X/SAM4E ports:

| File | Purpose |
|------|---------|
| `Kconfig` | `SOC_SERIES_SAM3U` → ARM, Cortex-M3, MPU, reset hook, poweroff |
| `Kconfig.soc` | `SOC_SERIES_SAM3U`, device `SOC_SAM3U4E`, `SOC_SERIES=sam3u` |
| `Kconfig.defconfig` | `NUM_IRQS = 47` |
| `CMakeLists.txt` | Sources `soc.c`; linker script = Cortex-M `linker.ld` |
| `sam3u.h` | Minimal device header (PMC, PIO, EFC, SUPC register blocks, peripheral IDs, CMSIS bit macros) |
| `cmsis_core.h` | Stand-in CMSIS core header (no Atmel CMSIS module is cloned in this tree): `SCB`, `NVIC`, `SysTick`, `IRQn_Type`, `NVIC_SetPriority`, intrinsics (`__WFI`, `__DMB`, `__set_MSP`, …) |
| `soc.h` | SoC macros, includes `sam3u.h` + common SAM helpers |
| `soc.c` | Clock init: internal fast RC → PLLA → MCK; `soc_reset_hook()` |

### Devicetree includes — `deps/zephyr/dts/arm/atmel/` (new)

| File | Purpose |
|------|---------|
| `sam3u.dtsi` | Series-level nodes: PMC, SRAM, EFC/flash controller, UART0, pinctrl + PIOA/B/C |
| `sam3u4e.dtsi` | Device-level flash (256 KB) and SRAM (48 KB) sizes |

### SoC registration — `deps/zephyr/soc/atmel/sam/soc.yml` (modified)

Added the `sam3u` series with the `sam3u4e` SoC so board/SoC validation (`scripts/list_boards.py`) resolves the board qualifier `sam3u4e`.

## What still needs attention for a physical board

1. **Confirm the LED pin** — `led0` is assigned to **PIOA pin 0** as a placeholder. Change
   `gpios = <&pioa 0 GPIO_ACTIVE_HIGH>` in `sam3u4e_ek.dts` to match the actual LED on your hardware (pin and active-high/low polarity).
2. **Flash the board** — Use the J-Link runner:
   `west flash -b sam3u4e_ek -r jlink` (device `AT91SAM3U4E`). A physical J-Link probe and the board are required; this was not tested on silicon here.
3. **UART console** — The console is disabled (`CONFIG_CONSOLE=n`) because this tree does not clone the Atmel CMSIS module and lacks the `SAM_PINMUX_*` pinctrl macros that `uart_sam`/`pinctrl_sam` need. blinky's `printf` therefore goes nowhere; the blinking LED is the observable output. Adding a console requires implementing the missing SAM pinctrl pinmux macros.
4. **Execution on silicon** — Verified build/link only; no SAM3U4E hardware was available for a runtime test.

## Build prerequisites (this environment)

- Zephyr SDK (arm-zephyr-eabi-gcc), CMake, Ninja, Python 3.12 with `jsonschema` and `pykwalify`
- Device-tree compiler `dtc`
- `BOARD_ROOT` must point at the directory containing `boards/` (here `zephyr-course/`)
