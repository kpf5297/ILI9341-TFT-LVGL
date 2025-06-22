/**
 * @file ILI9341.h
 * @brief Minimal driver definitions for the ILI9341 SPI display.
 */
#ifndef ILI9341_H
#define ILI9341_H

#include "stm32f4xx_hal.h"

#define ILI9341_SPI       hspi1

#define ILI9341_DC_PORT   GPIOB
#define ILI9341_DC_PIN    GPIO_PIN_6

#define ILI9341_RESET_PORT GPIOC
#define ILI9341_RESET_PIN  GPIO_PIN_7

#define ILI9341_CS_PORT   GPIOA
#define ILI9341_CS_PIN    GPIO_PIN_9

#define ILI9341_WIDTH     240
#define ILI9341_HEIGHT    320

void ILI9341_Init(void);
void ILI9341_FillScreen(uint16_t color);
void ILI9341_SetRotation(uint8_t rotation);
void ILI9341_WriteCommand(uint8_t cmd);
void ILI9341_WriteData(uint8_t data);
void ILI9341_WriteData16(uint16_t data);


//LVGL
void ILI9341_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ILI9341_DrawBitmap(uint16_t w, uint16_t h, uint8_t *s);
void ILI9341_DrawBitmapDMA(uint16_t w, uint16_t h, uint8_t *s);

#endif

