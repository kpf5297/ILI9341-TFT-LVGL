# tft_drivers — ILI9341 TFT + XPT2046 Touch, CubeMX-Decoupled, RTOS-Safe

Layered display and touch drivers for the 2.8" ILI9341 SPI TFT with XPT2046
resistive touchscreen. Layer 0 (OS abstraction + SPI/GPIO ports) and Layer 1
(chip drivers) are CubeMX-free and work identically on bare metal or
CMSIS-RTOS2/FreeRTOS; TouchGFX integration notes sit on top.

This directory is the library itself, vendored under `App/drivers/` of the
reference project one level up (see the [root README](../../README.md) for
how to build and run that project). Drop this same directory into
`App/drivers/` of your own project the same way — nothing in here knows
about the project it's vendored into except through `drv_constants.h`.

**Tested on:** STM32F446RE (Nucleo-64)
**GUI framework:** TouchGFX (see `adapters/touchgfx/README.md`)
**HAL:** STM32 HAL (CubeMX-generated), reached only through `drv_constants.h`

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│  Application (bare-metal loop / FreeRTOS tasks)           │
├───────────────────────────────────────────────────────────┤
│  adapters/touchgfx (GUI glue, generated per project)      │  Layer 2
├───────────────────────────────────────────────────────────┤
│  ili9341.c        xpt2046.c                               │  Layer 1 (chip drivers)
├───────────────────────────────────────────────────────────┤
│  drv_spi  drv_gpio  drv_os  drv_isr                       │  Layer 0 (ports + OSAL)
├───────────────────────────────────────────────────────────┤
│  STM32 HAL  ←  handles/pins supplied via drv_constants.h  │
└───────────────────────────────────────────────────────────┘
```

**Include-direction rule:** Layer 1 includes only Layer 0 headers + libc.
Layer 0 port implementations (`src/port/`, `src/drv_setup.c`,
`src/drv_isr_glue.c`) are the only files that see the STM32 HAL, and only via
`drv_config.h`. No layer includes CubeMX headers except through
`drv_constants.h`. This is
enforced by `cmake/layering_check_script.cmake`, wired into both the
`layering_check` build target and CTest.

---

## Repository layout

```text
App/drivers/                          # this directory — the tft_drivers library
├── CMakeLists.txt                    # tft_drivers static-library target (+ options)
├── include/drv/                      # public headers — #include "drv/xxx.h"
│   ├── drv_os.h / drv_spi.h / drv_gpio.h   # Layer 0 contracts
│   ├── drv_config.h                  # contract gate: pulls in drv_constants.h
│   ├── drv_isr.h                     # ISR entry points the app routes to
│   ├── drv_setup.h                   # one-call bring-up (DRV_Setup)
│   ├── ili9341.h / xpt2046.h         # Layer 1 chip drivers
├── src/
│   ├── os/                           # drv_os_cmsis2.c / drv_os_baremetal.c
│   ├── port/                         # drv_spi_stm32.c / drv_gpio_stm32.c
│   ├── ili9341.c / xpt2046.c
│   ├── drv_setup.c                   # \
│   └── drv_isr_glue.c                # / only files that reference DRV_* macros
├── adapters/
│   └── touchgfx/README.md            # integration notes + skeleton (no compiled code)
├── templates/drv_constants_template.h  # copy this to App/drv_constants.h
├── tests/
│   ├── host/                         # host unit tests (no ARM toolchain needed)
│   └── target/hardware_smoke_checklist.md
└── datasheets/
```

The project this library is vendored into (one level up — see the
[root README](../../README.md)) has this shape, which is also the shape any
project consuming this library should follow:

```text
<project-root>/
├── CMakeLists.txt                    # top-level glue (yours, never regenerated)
├── App/
│   ├── CMakeLists.txt
│   ├── drv_constants.h               # THE connector: DRV_* macros → CubeMX symbols
│   ├── app_main.c / app_main.h       # App_Init() / App_CreateTasks(), called from main.c
│   └── drivers/                      # this directory (submodule or copy)
└── Profiles/
    └── board_1/                      # 100% CubeMX-generated output for this board
        ├── board_1.ioc
        ├── Core/ (Inc, Src)          # main.c, stm32f4xx_it.c, FreeRTOSConfig.h, ...
        ├── Drivers/ (CMSIS, STM32F4xx_HAL_Driver)
        ├── Middlewares/ (FreeRTOS, ST/touchgfx)
        ├── TouchGFX/                 # TouchGFX Designer project + target HAL glue
        ├── cmake/stm32cubemx/CMakeLists.txt
        ├── startup_stm32f446xx.s
        └── STM32F446XX_FLASH.ld
```

**Rule:** nothing outside `Profiles/<name>/` is ever touched by CubeMX
regeneration, and the only lines added inside it live in `USER CODE` blocks
(see [Wiring App_Init/App_CreateTasks](#wiring-app_init--app_createtasks)
below — `main.c` and `freertos.c` each need exactly one include and one
function call there).

---

## The `drv_constants.h` contract

This is the **only** file, on either side of the tft_drivers/App boundary,
that includes a CubeMX header (`main.h`, `spi.h`, ...). Copy
`templates/drv_constants_template.h` to `App/drv_constants.h` and fill in
every macro — `drv_config.h` will `#error` with a clear message if any are
missing, and validates resolution/rotation/calibration ranges at compile
time.

| Macro | Meaning |
| --- | --- |
| `DRV_HAL_HEADER` | HAL family header string, e.g. `"stm32f4xx_hal.h"` |
| `DRV_DISP_SPI_MX_INIT()` / `DRV_TOUCH_SPI_MX_INIT()` | CubeMX re-init hooks (error recovery, baud change) |
| `DRV_DISP_SPI_HANDLE` | `&hspi1` or similar |
| `DRV_DISP_CS_PORT` / `_PIN`, `_DC_*`, `_RESET_*` | Display GPIO |
| `DRV_DISP_HOR_RES` / `_VER_RES` | Logical resolution after rotation |
| `DRV_DISP_ROTATION` | 0–3 |
| `DRV_TOUCH_SPI_HANDLE` | May equal `DRV_DISP_SPI_HANDLE` (shared bus — the driver serializes access) |
| `DRV_TOUCH_CS_PORT` / `_PIN`, `_IRQ_PORT` / `_PIN` | Touch GPIO |
| `DRV_TOUCH_IRQ_EXTI_LINE` | EXTI line number, for glue routing |
| `DRV_TOUCH_RAW_X_MIN/MAX`, `_Y_MIN/MAX` | Calibration defaults (overwritten at runtime by `XPT2046_SetCalibration`) |

`drv_setup.c` and `drv_isr_glue.c` are the **only** library sources that
reference these macros — everything else in Layers 0/1 consumes plain config
structs/handles, which is what makes them host-testable (see
[Testing](#testing)).

If your app needs its own peripherals beyond the display/touch (e.g. the
reference project this library is vendored into reads an RTC and sends UART
debug output), extend `drv_constants.h` with an app-level section using your
own prefix (the reference project uses `APP_*`) and put the code that
touches those raw handles in one small glue file (`../system.c`) — the rest
of your app should depend on that file's clean API, never on `main.h`
symbols directly.

---

## The `drv_platform` CMake contract

`tft_drivers`'s `CMakeLists.txt` doesn't know your CubeMX include paths, MCU
defines, or where `drv_constants.h` lives. Before `add_subdirectory()`-ing
this repo, define an `INTERFACE` target named `drv_platform`:

```cmake
add_library(drv_platform INTERFACE)
target_include_directories(drv_platform INTERFACE
    App/                                              # drv_constants.h
    Profiles/board_1/Core/Inc
    Profiles/board_1/Drivers/CMSIS/Include
    Profiles/board_1/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Profiles/board_1/Drivers/STM32F4xx_HAL_Driver/Inc
    Profiles/board_1/Middlewares/Third_Party/FreeRTOS/Source/include
    Profiles/board_1/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
)
target_compile_definitions(drv_platform INTERFACE STM32F446xx USE_HAL_DRIVER)

add_subdirectory(path/to/ILI9341-TFT-TouchGFX tft_drivers)
```

See `../CMakeLists.txt` (the `App/` directory of the
reference project this library is vendored into) for a complete working
example.

### CMake options

| Option | Default | Effect |
| --- | --- | --- |
| `DRV_USE_CMSIS_RTOS2` | `ON` | CMSIS-RTOS2 OSAL backend; `OFF` builds the bare-metal backend instead |
| `DRV_PROVIDE_HAL_CALLBACKS` | `ON` | Compile the default `HAL_SPI_TxCpltCallback`/`HAL_GPIO_EXTI_Callback` glue |
| `DRV_SPI_TIMEOUT_MS` | `100` | Blocking SPI transfer timeout |
| `DRV_SPI_MAX_BUSES` | `2` | Max concurrent `drv_spi_bus_t` objects |
| `ILI9341_MAX_LINE_PX` | `320` | Caps `ILI9341_FillScreen`'s stack line buffer |

---

## Bring-up recipe

1. **[CubeMX]** Configure clocks, SPI1 (display, DMA TX required), SPI2
   (touch), the four/five GPIO pins, FreeRTOS, and optionally RTC/UART — see
   [CubeMX peripheral setup](#cubemx-peripheral-setup) below. Generate into
   `Profiles/board_1/`.
2. **[Code]** Copy `templates/drv_constants_template.h` to
   `App/drv_constants.h` and fill in every macro for your board.
3. **[Code]** Set up `App/CMakeLists.txt` with the `drv_platform` contract
   above, then `add_subdirectory(<tft_drivers repo>)`.
4. **[Code]** Write `App/app_main.c` / `.h`:

```c
#include "drv/drv_setup.h"
#include "cmsis_os2.h"

/* TouchGFX entry points — plain C symbols, so App/ needs no TouchGFX
 * include paths. */
void MX_TouchGFX_Process(void);
void touchgfxSignalVSync(void);
void TouchGFXDisplayDriver_Init(void);

static void touchgfx_task(void *arg)
{
    (void)arg;
    /* Must run in task context: pre-kernel the HAL tick is dead, so the
     * driver's reset-pulse HAL_Delay() would never return. */
    if (DRV_Setup() != DRV_OK) {
        for (;;) { }
    }
    TouchGFXDisplayDriver_Init();
    MX_TouchGFX_Process(); /* never returns */
}

void App_CreateTasks(void)
{
    osThreadNew(touchgfx_task, NULL, &k_touchgfx_task_attr);
    /* plus an osTimerNew(...) firing touchgfxSignalVSync() at ~60 Hz to
     * pace the render loop — see the example for a full version */
}
```

### Wiring `App_Init` / `App_CreateTasks`

In `Profiles/board_1/Core/Src/main.c`, inside `/* USER CODE BEGIN 2 */`
(after peripheral init, before `osKernelStart()`):

```c
App_Init();
```

In `Profiles/board_1/Core/Src/freertos.c`'s `MX_FREERTOS_Init()`, inside
`/* USER CODE BEGIN RTOS_THREADS */`:

```c
App_CreateTasks();
```

Plus one `#include "app_main.h"` in each file's own `USER CODE BEGIN
Includes` block. `stm32f4xx_it.c` needs **no edits** — interrupt forwarding
is handled by the glue below.

> **Regenerating from CubeMX:** if your `.ioc` still has CubeMX-managed
> FreeRTOS tasks/timers from before you adopted this structure, remove them
> from the FreeRTOS task list in CubeMX so it stops re-emitting conflicting
> definitions into `freertos.c` outside the `USER CODE` blocks.

---

## Interrupt wiring

The driver owns ISR logic; no driver file defines `HAL_SPI_TxCpltCallback`
or `HAL_GPIO_EXTI_Callback` unconditionally. Two strategies, chosen by your
SPI HAL configuration:

- **HAL register-callbacks mode** (preferred): in CubeMX → Project Manager →
  Advanced Settings, enable "Register Callbacks" for your display's SPI
  peripheral. `DRV_Setup()` then registers its own
  `HAL_SPI_TX_COMPLETE_CB_ID` callback at init; `src/drv_isr_glue.c` defines
  nothing global.
- **Default HAL callbacks** (`USE_HAL_SPI_REGISTER_CALLBACKS` not set to 1):
  `src/drv_isr_glue.c` (compiled when `DRV_PROVIDE_HAL_CALLBACKS=ON`, the
  default) defines `HAL_SPI_TxCpltCallback`/`HAL_GPIO_EXTI_Callback`,
  filtered to the display SPI handle / touch IRQ pin, falling through
  silently otherwise. If your app defines either of these itself, set
  `DRV_PROVIDE_HAL_CALLBACKS=OFF` and forward to
  `DRV_ISR_DisplaySpiTxCplt()` / `DRV_ISR_TouchPenDown()`
  (`include/drv/drv_isr.h`) manually — otherwise you'll get a duplicate
  symbol at link time.

**NVIC contract:** the display SPI (or its DMA stream) and the touch EXTI
line must both be at NVIC priority ≥
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5 for the default FreeRTOS
config) — this is what makes it safe for their ISRs to call `drv_sem_give`.

---

## Bare-metal usage (no RTOS)

Set `DRV_USE_CMSIS_RTOS2=OFF`. `drv_delay_ms`/`drv_tick_ms` map to
`HAL_Delay`/`HAL_GetTick`, mutexes become a PRIMASK-guarded flag, and
semaphores a simple ISR-settable flag — all pre-kernel-safe by construction,
since there's no kernel. A minimal main loop:

```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init(); MX_DMA_Init(); MX_SPI1_Init(); MX_SPI2_Init();

    ili9341_t *disp; xpt2046_t *touch;
    DRV_Setup();
    disp = DRV_GetDisplay();
    touch = DRV_GetTouch();

    ILI9341_FillScreen(disp, 0xFFFF);
    for (;;) {
        uint16_t x, y;
        if (XPT2046_ReadPoint(touch, &x, &y, DRV_DISP_HOR_RES, DRV_DISP_VER_RES, DRV_DISP_ROTATION)) {
            ILI9341_DrawPixel(disp, x, y, 0x0000);
        }
    }
}
```

`ILI9341_FlushAsync`/`XPT2046_PenIRQ_ISR` still work the same way — you just
forward the HAL callbacks yourself (see [Interrupt wiring](#interrupt-wiring)) instead of relying on
a GUI task/kernel to consume them.

---

## TouchGFX

See `adapters/touchgfx/README.md` for a skeleton `HAL::flushFrameBuffer`
override and `TouchController::sampleTouch` implementation — TouchGFX
projects are generated by TouchGFX Designer, so there's no compiled adapter
target, just documentation. `drv_constants.h` and `DRV_Setup()` are the same
as for any other GUI stack; only the Layer 2 glue differs.

Touch calibration is the app's job: measure the raw corner values for your
panel once, apply them with `XPT2046_SetCalibration`, and read them back with
`XPT2046_GetCalibration` if you want to persist them.

---

## Testing

### Host unit tests (no ARM toolchain needed)

```bash
cd tests/host && ./run.sh
```

Builds `src/ili9341.c`/`src/xpt2046.c` against scriptable mocks of
`drv_spi.h`/`drv_gpio.h`/`drv_os.h` (this only works because Layers 0/1 never
see the STM32 HAL directly), the real bare-metal OSAL backend against a
wall-clock `HAL_Delay`/`HAL_GetTick` stand-in, and the `layering_check` rule
— all via CTest.

### Hardware smoke checklist

`tests/target/hardware_smoke_checklist.md` — boot, render, rotation, touch
corners, calibration, a 10-minute soak, plus a concurrent shared-bus stress
test and a DMA soak. This is the final gate before
trusting a new board bring-up; run it by hand on target.

---

## CubeMX peripheral setup

Steps marked **[CubeMX]** happen inside STM32CubeMX before code generation;
**[Code]** steps are manual edits after generation. Generate into
`Profiles/board_1/` (Project Manager → set the project location there).

### Step 1 — Clock Configuration [CubeMX]

1. Open the **Clock Configuration** tab.
2. Set SYSCLK to the maximum supported frequency for your MCU.
   - STM32F446RE: 180 MHz using HSE + PLL (PLLM=4, PLLN=180, PLLP=2).
   - Enable **Over-Drive** if running above 168 MHz.
3. Set APB1 ≤ 45 MHz, APB2 ≤ 90 MHz.
4. Verify SysTick source is HCLK (used by `HAL_Delay` and `HAL_GetTick`).

> If you skip over-drive and run at 180 MHz the chip will silently malfunction.

### Step 2 — SPI for the Display (ILI9341) [CubeMX]

The ILI9341 uses full-duplex SPI in Mode 0 (CPOL=0, CPHA=0).

1. Enable **SPI1** (or any SPI peripheral with a DMA-capable TX stream).
2. Mode: **Full-Duplex Master**, Data Size **8 Bits**, CPOL **Low** / CPHA **1
   Edge**, NSS **Software**, First Bit **MSB First**. Baud rate: start at
   **APB2 / 16** (~5.6 MHz) for bring-up; increase to /4 once confirmed.
3. Under **DMA Settings**: add a DMA request for **SPI1_TX**
   (DMA2 Stream 3 / Channel 3 on STM32F4xx), Memory→Peripheral, memory
   increment enabled, byte width, Normal mode.
4. Under **NVIC Settings**, enable the SPI1 and DMA2 global interrupts at
   priority **5** (≥ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`).
5. **Recommended:** Project Manager → Advanced Settings → enable "Register
   Callback" for SPI1, so `DRV_Setup()` can self-register its TX-complete
   callback instead of relying on the weak-symbol glue.

> SPI1 TX DMA is required for non-blocking display updates; without it,
> `ILI9341_FlushAsync` degrades to blocking behavior.

### Step 3 — SPI for the Touch Controller (XPT2046) [CubeMX]

Same SPI mode as the display, no DMA needed.

1. Enable **SPI2** (any spare SPI peripheral).
2. Baud rate: **APB1 / 8** (~5.6 MHz), or APB1/16 (2.8 MHz) if readings are
   noisy — the XPT2046's SPI clock limit is 2 MHz at 3.3 V.
3. Enable the SPI2 global interrupt at priority **5**.

### Step 4 — GPIO Pins [CubeMX]

Configure as **GPIO_Output**, push-pull, no pull, high-speed:

| Signal | User Label |
| --- | --- |
| ILI9341 CS | `CS` |
| ILI9341 DC | `DC` |
| ILI9341 RESET | `RESET` |
| XPT2046 CS | `T_CS` |

Configure as **GPIO_EXTI** (falling-edge trigger):

| Signal | User Label |
| --- | --- |
| XPT2046 IRQ (PENIRQ) | `T_IRQ` |

Enable the EXTI line for `T_IRQ` in **NVIC** at priority **5**.

> Set `T_IRQ` pull to **No pull** if the display board has a hardware
> pull-up (most do); **Pull-up** if unsure.

### Complete wiring note

The chip-select pins are active-low and should be left deasserted at reset, so
CubeMX should initialize `CS`, `T_CS`, and the other control outputs **high**.
The touch interrupt pin `T_IRQ` is also active-low: configure it as a
falling-edge EXTI input, with a pull-up if the board does not provide one.
Display SPI TX-complete and touch EXTI callbacks are forwarded through the
driver ISR hooks described in [Interrupt wiring](#interrupt-wiring); `main.c`
and `freertos.c` only need the `App_*` calls shown above.

### Step 5 — FreeRTOS [CubeMX]

1. Enable **FreeRTOS** under **Middleware** → **CMSIS-RTOS V2** API.
2. Heap Management: **Heap_4**, Total Heap Size ≥ 32 KB, `USE_MUTEXES`,
   `USE_RECURSIVE_MUTEXES`, `USE_COUNTING_SEMAPHORES` all enabled.
3. Don't create GUI/app tasks in CubeMX's FreeRTOS task list — they live in
   `App_CreateTasks()` instead, outside the regeneration zone.

Verify in `Core/Inc/FreeRTOSConfig.h`:

```c
#define configENABLE_FPU                              1     /* Cortex-M4F */
#define configTOTAL_HEAP_SIZE                         (32 * 1024)
#define configTICK_RATE_HZ                             1000
#define configUSE_MUTEXES                               1
#define configUSE_RECURSIVE_MUTEXES                     1
#define configUSE_COUNTING_SEMAPHORES                   1
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY     5
```

### Step 6 — Optional: RTC and UART [CubeMX]

Only needed for the example's time/date display and UART debug output.

- **RTC**: internal LSE (accurate) or LSI (bring-up without a crystal).
- **UART2 with DMA**: Asynchronous, 115200 baud, DMA TX request enabled.

### Step 7 — Generate Code [CubeMX]

Set **Toolchain/IDE** to **CMake**, target location `Profiles/board_1/`,
click **Generate Code**.

---

## Rotation reference

| Value | Mode | HOR_RES | VER_RES |
| --- | --- | --- | --- |
| 0 | Portrait | 240 | 320 |
| 1 | Portrait flipped | 240 | 320 |
| 2 | Landscape flipped | 320 | 240 |
| 3 | Landscape | 320 | 240 |

Set `DRV_DISP_ROTATION` and matching `DRV_DISP_HOR_RES`/`DRV_DISP_VER_RES` in
`drv_constants.h`. Rotation 3 is the one validated against real hardware in
this repo's history; 0–2 use an analytically-derived touch mapping (see
`src/xpt2046.c`) that should be re-checked on your panel via the hardware
smoke checklist.

---

## Porting to a new board

1. `cp Profiles/board_1 Profiles/board_2` — or generate fresh from CubeMX
   into `Profiles/board_2/`.
2. Copy and edit `App/drv_constants.h` for the new pin/handle mapping (or
   keep one `drv_constants.h` per profile if the app differs too).
3. Build with `-DPROFILE=board_2`.

No changes to `tft_drivers` itself are needed — board variation lives
entirely in `drv_constants.h` content.

---

## License

MIT
