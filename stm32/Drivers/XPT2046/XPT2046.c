#include "XPT2046.h"

/* Command bytes */
#define CMD_X   0xD0
#define CMD_Y   0x90

/* Helpers */
#define CS_LOW()   HAL_GPIO_WritePin(XPT2046_CS_PORT, XPT2046_CS_PIN, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(XPT2046_CS_PORT, XPT2046_CS_PIN, GPIO_PIN_SET)

static SPI_HandleTypeDef *xpt_spi = NULL;

/* Reads one 12-bit value (throws away bottom 4 bits) */
static uint16_t spi_xfer(uint8_t cmd) {
    uint8_t tx[3] = { cmd, 0, 0 }, rx[3];
    HAL_SPI_TransmitReceive(xpt_spi, tx, rx, 3, HAL_MAX_DELAY);
    return (((uint16_t)rx[1] << 8) | rx[2]) >> 4;
}

void XPT2046_Init(SPI_HandleTypeDef *hspi) {
    xpt_spi = hspi;
    CS_HIGH();
}

bool XPT2046_TouchDetected(void) {
    /* PENIRQ is wired: only true when you actually touch */
    return HAL_GPIO_ReadPin(XPT2046_IRQ_PORT, XPT2046_IRQ_PIN)
           == GPIO_PIN_RESET;
}

bool XPT2046_GetTouch(uint16_t *x, uint16_t *y) {
    if(!XPT2046_TouchDetected()) return false;

    CS_LOW();
    /* throw away one to stagger timing, then read two and average */
    spi_xfer(CMD_X);
    uint16_t x1 = spi_xfer(CMD_X);
    uint16_t y1 = spi_xfer(CMD_Y);
    uint16_t x2 = spi_xfer(CMD_X);
    uint16_t y2 = spi_xfer(CMD_Y);
    CS_HIGH();

    *x = (x1 + x2) >> 1;
    *y = (y1 + y2) >> 1;
    return true;
}
