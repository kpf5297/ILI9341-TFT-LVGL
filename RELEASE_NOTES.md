# Release Notes — v3.0.0

**LVGL removed — TouchGFX-only reference project**

---

## Overview

TouchGFX has been the live UI since v2.x, with LVGL kept in the tree but
never compiled (`DRV_ADAPTER_LVGL` was forced `OFF`). v3.0.0 deletes that
dead path entirely: the repository is now a TouchGFX + `tft_drivers`
reference project, and `tft_drivers` is a GUI-framework-agnostic library
with TouchGFX integration notes.

No runtime behavior changes. Display flush, touch sampling, and the ~60 Hz
VSync pacing were already handled by
`Profiles/board_1/TouchGFX/target/TouchGFXDisplayDriver.cpp`,
`STM32TouchController.cpp`, and `App/app_main.c` respectively.

## Breaking changes

| Removed | Replacement |
| --- | --- |
| `DRV_ADAPTER_LVGL` CMake option | None — `adapters/lvgl/` is deleted; TouchGFX glue lives in your TouchGFX project's `target/` folder |
| `DRV_DROP_FRAME_IF_BUSY`, `DRV_DMA_BUSY_WAIT_TIMEOUT_MS` CMake options and `include/drv/drv_policy.h` | Implement flush policy in the GUI glue using `ILI9341_FlushBusy()` / `ILI9341_WaitFlushDone()` |
| `lv_ili9341_create()`, `lv_xpt2046_create()`, `lv_xpt2046_set_wake_cb()` | `ILI9341_FlushAsync()` / `XPT2046_ReadPoint()` called from the GUI glue — see `App/drivers/adapters/touchgfx/README.md` |
| `lv_xpt2046_calibrate()` | Measure the raw corners in your app and apply them with `XPT2046_SetCalibration()`; `XPT2046_GetCalibration()` reads them back for persisting |
| `App/lvgl/` (vendored LVGL v9.2.x), `App/lv_conf.h`, `App/gui/` | `Profiles/board_1/TouchGFX/` (Designer project, generated UI, board glue) |
| `lvgl.h` layering-check rule | Dropped; the CubeMX-include and HAL-free-header rules are unchanged |

`App/drivers/REWORK_PLAN.md` (the v2.0.0 design document) was also removed.

## Upgrading

Consumers vendoring `tft_drivers` who were building with
`DRV_ADAPTER_LVGL=ON` should stay on v2.x, or port their display/touch
bindings to the pattern in `App/drivers/adapters/touchgfx/README.md`.
Everything below Layer 2 — `DRV_Setup()`, `drv_constants.h`, the SPI/GPIO
ports, the OSAL, and the chip drivers — is unchanged.

---

# Release Notes — v2.0.0

**tft_drivers — ILI9341 TFT + XPT2046 Touch, CubeMX-Decoupled, RTOS-Safe**

---

## Overview

v2.0.0 is a ground-up rework: the driver is no longer a `Board/`+`Display/`
pair of CubeMX-coupled files, but a layered, CubeMX-free static library
(`tft_drivers`) with a single connector header (`drv_constants.h`),
TouchGFX integration notes, host-runnable unit tests,
and a restructured `App/` + `Profiles/board_1/` example project. This
section only covers what changed for existing users.

## Breaking changes

Everything below is a rename or a structural move; behavior (init byte
sequence, MADCTL values, touch sampling order) is unchanged except where
noted as a fix in `App/drivers/REWORK_PLAN.md` Appendix A.

| Old (v1.x) | New (v2.0.0) |
| --- | --- |
| `Display/ILI9341.c`, `Board/board_drivers.c`, ... | `App/drivers/src/*.c` + `App/drivers/include/drv/*.h` (the `tft_drivers` CMake target) |
| `Board/board_config.h` | `App/drivers/include/drv/drv_config.h` |
| `Board/boards/board_*.h` | `App/drv_constants.h` (copied from `App/drivers/templates/drv_constants_template.h`) |
| `BOARD_*` macros | `DRV_*` macros (same meaning, new prefix; `DRV_HAL_HEADER`, `DRV_DISP_SPI_MX_INIT()`, `DRV_TOUCH_SPI_MX_INIT()`, `DRV_TOUCH_IRQ_EXTI_LINE` are new/required) |
| `#define FREERTOS` | `DRV_USE_CMSIS_RTOS2` CMake option (default `ON`); no more `#ifdef FREERTOS` in driver code |
| `ILI9341_Init(config)` (no instance) | `ILI9341_Init(&ili9341_t*, cfg)` — opaque instance pointer |
| `ILI9341_DrawBitmapDMA(w, h, s, display)` | `ILI9341_FlushAsync(d, x0,y0,x1,y1, px, len, done_cb, user)` — no GUI-framework type in the driver, handles chunking for full-frame transfers |
| `ILI9341_CancelFlushCallback()` | Not needed — the GUI glue's pending-pointer pattern closes the double-completion race directly |
| global `HAL_SPI_TxCpltCallback` / `HAL_GPIO_EXTI_Callback` in driver `.c` files | `App/drivers/src/drv_isr_glue.c` (toggle with `DRV_PROVIDE_HAL_CALLBACKS`) or HAL register-callbacks mode; see `App/drivers/README.md` "Interrupt wiring" |
| `TouchController_Init(cfg)` / `TouchController_Poll()` | `XPT2046_ReadPoint(t, &x, &y, hor, ver, rotation)` — called directly from the GUI glue, no separate poll call |
| `touch_calibrate()` (printf-based) | Removed — measure the raw corners in your app and apply them with `XPT2046_SetCalibration` / read back with `XPT2046_GetCalibration` |
| `Board_Get*()` / `Board_Set*()` | Deleted — config is a plain struct passed at `*_Init()`, calibration via `XPT2046_SetCalibration`/`GetCalibration` |
| `display_runtime_config.h` `DISPLAY_*` knobs | Flush policy moved into the GUI glue; the driver exposes `ILI9341_FlushBusy` / `ILI9341_WaitFlushDone` instead |
| `examples/01_Display/{Core,Drivers,Middlewares,...}` at the example root | `Profiles/board_1/{Core,Drivers,Middlewares,...}`; app code moved to `App/` (`app_main.c`) |
| `BOARD_TARGET` CMake cache var | `PROFILE` CMake cache var, selecting `Profiles/${PROFILE}` |

Fixed as part of this rework: the
CS-before-BSY-clear truncation, the `uint16_t` DMA length overflow on
full-frame flushes, the missing `ILI9341_DrawPixel` implementation, the
un-locked shared SPI bus between display and touch, the single-rotation-only
touch axis mapping, and pre-kernel `osDelay`/mutex hangs.

## Known limitations carried forward

- Touch calibration is still not persisted across power cycles by the
  library itself — `XPT2046_SetCalibration`/`GetCalibration` give you the
  values; persisting them (flash, EEPROM, ...) is up to the app.
- Rotation 3 (landscape) is the only touch mapping validated against real
  hardware in this repo's history; rotations 0–2 are analytically derived
  (see `App/drivers/README.md` "Rotation reference") and should be
  reconfirmed on your panel.
- `XPT2046_ReadRaw`/`PenDown` still require a wired PENIRQ GPIO; IRQ-less
  polling is not supported.

---

# Release Notes — v2.1.0

**Repository restructure: sample project promoted to repo root**

---

## Overview

v2.1.0 makes no code changes — it inverts the repository's top-level shape.
Previously the `tft_drivers` library lived at the repo root and the
reference application lived under `examples/01_Display/`. That
was backwards for a repo whose entire point is demonstrating the reference
application: the library is now vendored at `App/drivers/` (exactly the
shape `App/drivers/README.md` already documented for consumers of this
library), and `App/` + `Profiles/board_1/` sit at the repo root.

| Old (v2.0.0) | New (v2.1.0) |
| --- | --- |
| `CMakeLists.txt`, `include/`, `src/`, `adapters/`, `templates/`, `cmake/`, `tests/`, `datasheets/` at repo root | same, under `App/drivers/` |
| `README.md` (library docs) at repo root | `App/drivers/README.md`; repo-root `README.md` now documents the reference project |
| `examples/01_Display/{App,Profiles,CMakeLists.txt,CMakePresets.json}` | `App/`, `Profiles/`, `CMakeLists.txt`, `CMakePresets.json` at repo root |

No driver source, CMake option, or `drv_constants.h` macro changed — this is
a directory move plus path fixups in `App/CMakeLists.txt`
(`TFT_DRIVERS_ROOT`) and the docs.

---

# Release Notes — v1.0.0

**ILI9341 TFT + XPT2046 Touch Driver for STM32**

---

## Overview

v1.0.0 is the first structured release of this driver package. The primary goal of this
release was to bring the driver up to production quality and restructure the repository so
users can add a single folder to their `Drivers/` directory and be running in minutes.

---

## What's New

### Repository restructured as a drop-in driver package

The entire repo is now the driver package. Add it to any CubeMX-generated project by
cloning or copying it into your `Drivers/` folder:

```bash
git submodule add <url> Drivers/ILI9341-TFT-TouchGFX
```

Driver source files now live at the repo root in two clean directories:

```
Display/    ILI9341, LCDController, XPT2046, TouchController, display_runtime_config
Board/      board_config, board_drivers, boards/board_stm32f446, boards/board_stm32f411
```

### Board abstraction layer

A new `Board/` module decouples hardware pin assignments from driver logic. All SPI
handles, GPIO ports/pins, display resolution, rotation, and touch calibration defaults are
defined in a single board header that maps directly to CubeMX-generated symbols.

- `board_config.h` — compile-time contract: missing or invalid `BOARD_*` macros are
  caught as `#error` before linking.
- `board_drivers.h / .c` — typed getters (`Board_GetDisplayConfig()`,
  `Board_GetTouchConfig()`, etc.) provide clean access to board resources.
- Runtime touch calibration override via `Board_SetTouchCalibration()` /
  `Board_ResetTouchCalibration()`.

**Board targets:** `stm32f446` (tested), `stm32f411` (template provided).

### DMA-backed display flush with deterministic timing policy

The display flush path (`LCDController.c`) uses SPI DMA for non-blocking frame transfers.
A compile-time policy (`display_runtime_config.h`) controls behavior when a DMA transfer
is still in-flight when the GUI requests the next flush:

| Policy                           | Behavior                                               |
| -------------------------------- | ------------------------------------------------------ |
| `DISPLAY_DROP_FRAME_IF_DMA_BUSY` | Drop the frame immediately — zero blocking, bounded latency |
| DMA busy-wait (timeout)          | Wait up to `DISPLAY_DMA_BUSY_WAIT_TIMEOUT_MS` ms, then give up |

All knobs default to deterministic (drop-frame) mode and can be overridden from CMake
without touching driver source.

### 4-point touch calibration

`touch_calibrate()` displays green crosshairs at the four screen corners and walks the
user through a guided tap sequence. Raw ADC values are printed via `printf` so you can
update `BOARD_TOUCH_RAW_*` defaults in your board header. Runtime calibration persists via
`Board_SetTouchCalibration()` without requiring a reflash.

### Comprehensive from-scratch setup guide

The root `README.md` is a single, self-contained 13-step guide covering:

- Clock and SPI configuration in CubeMX (with exact register values for STM32F446)
- DMA setup for non-blocking display transfers
- FreeRTOS heap sizing and `FreeRTOSConfig.h` key values
- CMake integration snippets ready to paste
- GUI task structure and thread-safety
- Hardware smoke checklist for first-boot verification

---

## Compatibility

| Component        | Version / Part        |
| ---------------- | --------------------- |
| MCU (tested)     | STM32F446RE (Nucleo-64) |
| MCU (template)   | STM32F411             |
| Display IC       | ILI9341 (2.8" SPI TFT, 320×240) |
| Touch IC         | XPT2046               |
| HAL              | STM32 HAL (CubeMX-generated) |
| RTOS             | FreeRTOS via CMSIS-RTOS V2 |
| Build system     | CMake 3.22+           |
| C standard       | C11                   |

---

## Breaking Changes

This is the first versioned release. Projects using the earlier unstructured layout
(`src/ILI9341.c`, `src/LCDController.c`, etc.) will need to update their CMake include
paths and add the board header for their target MCU.

---

## Known Limitations

- Touch calibration values are not persisted across power cycles (no NVM/flash write). The
  four `BOARD_TOUCH_RAW_*` defaults in the board header are the permanent baseline.
- Only portrait and landscape rotations are calibrated and tested; flipped modes (1 and 2)
  may require touch axis remapping adjustments.
- `XPT2046_TouchDetected()` requires a wired PENIRQ GPIO. IRQ-less polling is not
  supported without modifying the driver.
