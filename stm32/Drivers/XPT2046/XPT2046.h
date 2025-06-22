/**
 * XPT2046.h ― ultra‑small polling driver for LVGL
 * (c) Kevin Fox 2025
 */
#ifndef XPT2046_H_
#define XPT2046_H_

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include "main.h"

/* ---------- user‑configurable pins ----------------------------------- */
#define XPT2046_CS_PORT     GPIOA
#define XPT2046_CS_PIN      GPIO_PIN_8

/*  Uncomment if the PENIRQ line is wired (low while pressed)            */
#define XPT2046_IRQ_PORT    T_IRQ_GPIO_Port
#define XPT2046_IRQ_PIN     T_IRQ_Pin
/* --------------------------------------------------------------------- */

void XPT2046_Init(SPI_HandleTypeDef *spi);
bool XPT2046_TouchDetected(void);
bool XPT2046_GetTouch(uint16_t *x, uint16_t *y);

#endif /* XPT2046_H_ */


