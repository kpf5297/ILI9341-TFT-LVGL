
# ILI9341 TFT Display with XPT2046 Touchscreen (STM32 + LVGL)

This project provides lightweight display and touch drivers for the 2.8" ILI9341 SPI display with XPT2046 touchscreen, including LVGL integration.

## Features

- ILI9341 SPI Display (STM32 HAL)
- XPT2046 resistive touchscreen with on-screen calibration
- Integrated with [LVGL](https://lvgl.io) (v9+)
- Easy drop-in glue code for STM32 projects
- Tested on STM32F446RE
- ESP32 port coming soon

---

## Directory Structure

```

stm32/
├── Core/
│   ├── LCDController.c/h       # Display glue for LVGL
│   ├── TouchController.c/h     # Touch glue for LVGL
├── Drivers/
│   ├── ILI9341/                # Display driver (HAL-based)
│   └── XPT2046/                # Touch driver (SPI-based)

````

---

## Getting Started

### 1. STM32CubeMX Configuration

- Enable SPI peripheral (e.g., SPI1 or SPI2)
- Add GPIO outputs for:
  - **ILI9341**: `CS`, `DC`, `RESET`
  - **XPT2046**: `CS`
- Enable EXTI interrupt for XPT2046 `IRQ` pin
- Enable `SysTick` and `FreeRTOS` (optional)

---

### 2. LVGL Setup

#### In `main.c`:

```c
lv_init();                  // Initialize LVGL core
lv_port_disp_init();        // Setup display driver (in LCDController.c)
TouchController_Init();     // Setup touch driver (in TouchController.c)
touch_calibrate();          // Optional: on-screen calibration UI
````

#### In `SysTick_Handler()`:

```c
void SysTick_Handler(void) {
    HAL_IncTick();
    lv_tick_inc(1);         // Must be called every 1 ms
}
```

---

### 3. CMake Integration (if using)

In `lv_conf.h`:

```c
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   1   // Required for correct ILI9341 color order
```


## License

MIT License

