#include "ILI9341.h"
#include "main.h"

extern SPI_HandleTypeDef ILI9341_SPI;

#define CS_LOW()  HAL_GPIO_WritePin(ILI9341_CS_PORT, ILI9341_CS_PIN, GPIO_PIN_RESET)
#define CS_HIGH() HAL_GPIO_WritePin(ILI9341_CS_PORT, ILI9341_CS_PIN, GPIO_PIN_SET)

#define DC_LOW()  HAL_GPIO_WritePin(ILI9341_DC_PORT, ILI9341_DC_PIN, GPIO_PIN_RESET)
#define DC_HIGH() HAL_GPIO_WritePin(ILI9341_DC_PORT, ILI9341_DC_PIN, GPIO_PIN_SET)

#define RESET_LOW()  HAL_GPIO_WritePin(ILI9341_RESET_PORT, ILI9341_RESET_PIN, GPIO_PIN_RESET)
#define RESET_HIGH() HAL_GPIO_WritePin(ILI9341_RESET_PORT, ILI9341_RESET_PIN, GPIO_PIN_SET)

static void ILI9341_Write(uint8_t *data, uint16_t size) {
    HAL_SPI_Transmit(&ILI9341_SPI, data, size, HAL_MAX_DELAY);
}

void ILI9341_WriteCommand(uint8_t cmd) {
    DC_LOW();
    CS_LOW();
    ILI9341_Write(&cmd, 1);
    CS_HIGH();
}

void ILI9341_WriteData(uint8_t data) {
    DC_HIGH();
    CS_LOW();
    ILI9341_Write(&data, 1);
    CS_HIGH();
}

void ILI9341_WriteData16(uint16_t data) {
    uint8_t buf[2] = {data >> 8, data & 0xFF};
    DC_HIGH();
    CS_LOW();
    ILI9341_Write(buf, 2);
    CS_HIGH();
}

void ILI9341_Reset(void) {
    RESET_LOW();
    HAL_Delay(20);
    RESET_HIGH();
    HAL_Delay(150);
}

void ILI9341_Init(void) {
    ILI9341_Reset();

    ILI9341_WriteCommand(0x01);
    HAL_Delay(10);
    ILI9341_WriteCommand(0x28);

    ILI9341_WriteCommand(0xCF);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0xC1);
    ILI9341_WriteData(0x30);

    ILI9341_WriteCommand(0xED);
    ILI9341_WriteData(0x64);
    ILI9341_WriteData(0x03);
    ILI9341_WriteData(0x12);
    ILI9341_WriteData(0x81);

    ILI9341_WriteCommand(0xE8);
    ILI9341_WriteData(0x85);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x78);

    ILI9341_WriteCommand(0xCB);
    ILI9341_WriteData(0x39);
    ILI9341_WriteData(0x2C);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x34);
    ILI9341_WriteData(0x02);

    ILI9341_WriteCommand(0xF7);
    ILI9341_WriteData(0x20);

    ILI9341_WriteCommand(0xEA);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x00);

    ILI9341_WriteCommand(0xC0);
    ILI9341_WriteData(0x23);

    ILI9341_WriteCommand(0xC1);
    ILI9341_WriteData(0x10);

    ILI9341_WriteCommand(0xC5);
    ILI9341_WriteData(0x3e);
    ILI9341_WriteData(0x28);

    ILI9341_WriteCommand(0xC7);
    ILI9341_WriteData(0x86);

//    ILI9341_WriteCommand(0x36);
//    ILI9341_WriteData(0x48);

    ILI9341_WriteCommand(0x3A);
    ILI9341_WriteData(0x55);

    ILI9341_WriteCommand(0xB1);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x18);

    ILI9341_WriteCommand(0xB6);
    ILI9341_WriteData(0x08);
    ILI9341_WriteData(0x82);
    ILI9341_WriteData(0x27);

    ILI9341_WriteCommand(0xF2);
    ILI9341_WriteData(0x00);

    ILI9341_WriteCommand(0x26);
    ILI9341_WriteData(0x01);

    ILI9341_WriteCommand(0x11);
    HAL_Delay(120);
    ILI9341_WriteCommand(0x29);

    ILI9341_SetRotation(3);
}

void ILI9341_FillScreen(uint16_t color) {
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    uint8_t buf[ILI9341_WIDTH * 2];

    for (int i = 0; i < ILI9341_WIDTH; ++i) {
        buf[2 * i] = hi;
        buf[2 * i + 1] = lo;
    }

    ILI9341_WriteCommand(0x2A);
    ILI9341_WriteData(0x00); ILI9341_WriteData(0);
    ILI9341_WriteData(0x00); ILI9341_WriteData(ILI9341_WIDTH - 1);

    ILI9341_WriteCommand(0x2B);
    ILI9341_WriteData(0x00); ILI9341_WriteData(0);
    ILI9341_WriteData(0x01); ILI9341_WriteData(ILI9341_HEIGHT - 1);

    ILI9341_WriteCommand(0x2C);
    DC_HIGH();
    CS_LOW();
    for (int y = 0; y < ILI9341_HEIGHT; y++) {
        HAL_SPI_Transmit(&ILI9341_SPI, buf, ILI9341_WIDTH * 2, HAL_MAX_DELAY);
    }
    CS_HIGH();
}

void ILI9341_SetRotation(uint8_t m)
{
    ILI9341_WriteCommand(0x36);
    switch (m) {
        case 0:
            ILI9341_WriteData(0x48); // MX | BGR
            break;
        case 1:
            ILI9341_WriteData(0x28); // MY | BGR
            break;
        case 2:
            ILI9341_WriteData(0x88); // MX | MY | BGR
            break;
        case 3:
            ILI9341_WriteData(0xE8); // MX | MY | MV | BGR (landscape)
            break;
    }
}


// LVGL
void ILI9341_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    ILI9341_WriteCommand(0x2A);
    ILI9341_WriteData(x0 >> 8);
    ILI9341_WriteData(x0 & 0xFF);
    ILI9341_WriteData(x1 >> 8);
    ILI9341_WriteData(x1 & 0xFF);

    ILI9341_WriteCommand(0x2B);
    ILI9341_WriteData(y0 >> 8);
    ILI9341_WriteData(y0 & 0xFF);
    ILI9341_WriteData(y1 >> 8);
    ILI9341_WriteData(y1 & 0xFF);

    ILI9341_WriteCommand(0x2C);
}

void ILI9341_DrawBitmap(uint16_t w, uint16_t h, uint8_t *s)
{
    ILI9341_WriteCommand(0x2C); // Memory write
    DC_HIGH();
    CS_LOW();
    HAL_SPI_Transmit(&ILI9341_SPI, s, w * h * 2, HAL_MAX_DELAY);
    CS_HIGH();
}

void ILI9341_DrawBitmapDMA(uint16_t w, uint16_t h, uint8_t *s)
{
    ILI9341_WriteCommand(0x2C); // Memory write
    DC_HIGH();
    CS_LOW();
    HAL_SPI_Transmit_DMA(&ILI9341_SPI, s, w * h * 2);
}

