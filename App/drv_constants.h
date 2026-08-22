#ifndef DRV_CONSTANTS_H_
#define DRV_CONSTANTS_H_

/*
 * The one and only connection point between App/ and this CubeMX project
 * (Profiles/board_1/). Nothing else under App/ may #include "main.h" or any
 * other CubeMX-generated header — every peripheral App code touches is
 * named here first.
 *
 * Two sections:
 *   - DRV_*: the tft_drivers library's contract (see
 *     templates/drv_constants_template.h in the tft_drivers repo).
 *   - APP_*: this example's own peripheral map (RTC/UART/LED for the demo
 *     clock task), consumed only by App/system.c.
 */

#include "main.h"
#include "spi.h"
#include "rtc.h"
#include "usart.h"
#include "iwdg.h"

/* ---- HAL family --------------------------------------------------------- */
#define DRV_HAL_HEADER               "stm32f4xx_hal.h"

/* ---- CubeMX init hooks --------------------------------------------------- */
#define DRV_DISP_SPI_MX_INIT()       MX_SPI1_Init()
#define DRV_TOUCH_SPI_MX_INIT()      MX_SPI2_Init()

/* ---- Display (ILI9341 on SPI1) ------------------------------------------- */
#define DRV_DISP_SPI_HANDLE          (&hspi1)
#define DRV_DISP_CS_PORT             CS_GPIO_Port
#define DRV_DISP_CS_PIN              CS_Pin
#define DRV_DISP_DC_PORT             DC_GPIO_Port
#define DRV_DISP_DC_PIN              DC_Pin
#define DRV_DISP_RESET_PORT          RESET_GPIO_Port
#define DRV_DISP_RESET_PIN           RESET_Pin
#define DRV_DISP_HOR_RES             320u   /* logical width after rotation  */
#define DRV_DISP_VER_RES             240u   /* logical height after rotation */
#define DRV_DISP_ROTATION            3u     /* 0=portrait  3=landscape       */

/* ---- Touch (XPT2046 on SPI2) ---------------------------------------------- */
#define DRV_TOUCH_SPI_HANDLE         (&hspi2)
#define DRV_TOUCH_CS_PORT            T_CS_GPIO_Port
#define DRV_TOUCH_CS_PIN             T_CS_Pin
#define DRV_TOUCH_IRQ_PORT           T_IRQ_GPIO_Port
#define DRV_TOUCH_IRQ_PIN            T_IRQ_Pin
#define DRV_TOUCH_IRQ_EXTI_LINE      4u     /* T_IRQ is EXTI4 on this board's pinout */

/* ---- Touch calibration defaults (raw ADC values at each screen edge) ----
 * Per-panel values are app-supplied: measure the raw corners once and feed
 * them to XPT2046_SetCalibration() at runtime (XPT2046_GetCalibration()
 * reads them back for persisting). */
#define DRV_TOUCH_RAW_X_MIN          262u
#define DRV_TOUCH_RAW_X_MAX          1872u
#define DRV_TOUCH_RAW_Y_MIN          230u
#define DRV_TOUCH_RAW_Y_MAX          1872u

/* ===========================================================================
 * App-level peripheral map (this example's clock/debug demo, not part of
 * the tft_drivers contract). Board-variant guards go here if App/ ever
 * needs to support more than one Profiles/<board> target.
 * ===========================================================================
 */
#define APP_RTC_HANDLE                (&hrtc)
#define APP_UART_HANDLE               (&huart2)
#define APP_DEBUG_LED_PORT            GREEN_LED_GPIO_Port
#define APP_DEBUG_LED_PIN             GREEN_LED_Pin
#define APP_IWDG_HANDLE               (&hiwdg)

#endif /* DRV_CONSTANTS_H_ */
