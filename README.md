cat <<EOF > README.md
# ILI9341 TFT Display with XPT2046 Touchscreen (STM32 + LVGL)

This project provides lightweight display and touch drivers for the 2.8" ILI9341 SPI display with XPT2046 touchscreen, including LVGL integration.

## Features

- ILI9341 SPI Display (HAL)
- XPT2046 resistive touch with on-screen calibration
- LVGL integration
- STM32 HAL (tested on STM32F446RE)
- ESP32 port coming soon

## Structure

```bash
stm32/
├── Core/
│   ├── LCDController.c/h       # Display glue for LVGL
│   ├── TouchController.c/h     # Touch glue for LVGL
├── Drivers/
│   ├── ILI9341/
│   └── XPT2046/
```

## Getting Started

1. Configure SPI, GPIO, and EXTI in STM32CubeMX.
2. Call these in main():

```c
lv_init();
lv_port_disp_init();
TouchController_Init();
touch_calibrate(); // optional
```

## License

MIT License © 2025 Kevin Fox

EOF
