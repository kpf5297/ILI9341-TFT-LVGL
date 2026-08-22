#ifndef DRV_CONSTANTS_H_
#define DRV_CONSTANTS_H_

/*
 * Copy this file to App/drv_constants.h in your project and fill in the
 * values for your board. This is the ONLY file where CubeMX-generated
 * headers (main.h, spi.h, ...) may be included by name — it is the single
 * connection point between the tft_drivers library and your CubeMX project.
 *
 * Every macro below is required; drv_config.h will #error with a clear
 * message if any is missing.
 */

#include "main.h"
#include "spi.h"

/* ---- HAL family -----------------------------------------------------
 * Header included by drv_config.h to bring in HAL types/macros. */
#define DRV_HAL_HEADER               "stm32f4xx_hal.h"

/* ---- CubeMX init hooks ------------------------------------------------
 * Allow the driver/app to re-init a peripheral (e.g. for error recovery or
 * a baud-rate change) without knowing the CubeMX-generated function names. */
#define DRV_DISP_SPI_MX_INIT()       MX_SPI1_Init()
#define DRV_TOUCH_SPI_MX_INIT()      MX_SPI2_Init()

/* ---- Display (ILI9341) ------------------------------------------------ */
#define DRV_DISP_SPI_HANDLE          (&hspi1)
#define DRV_DISP_CS_PORT             CS_GPIO_Port
#define DRV_DISP_CS_PIN              CS_Pin
#define DRV_DISP_DC_PORT             DC_GPIO_Port
#define DRV_DISP_DC_PIN              DC_Pin
#define DRV_DISP_RESET_PORT          RESET_GPIO_Port
#define DRV_DISP_RESET_PIN           RESET_Pin
#define DRV_DISP_HOR_RES             320u
#define DRV_DISP_VER_RES             240u
#define DRV_DISP_ROTATION            3u

/* ---- Touch (XPT2046) --------------------------------------------------- */
#define DRV_TOUCH_SPI_HANDLE         (&hspi2)   /* may equal DRV_DISP_SPI_HANDLE */
#define DRV_TOUCH_CS_PORT            T_CS_GPIO_Port
#define DRV_TOUCH_CS_PIN             T_CS_Pin
#define DRV_TOUCH_IRQ_PORT           T_IRQ_GPIO_Port
#define DRV_TOUCH_IRQ_PIN            T_IRQ_Pin
#define DRV_TOUCH_IRQ_EXTI_LINE      4u          /* EXTI line number for glue routing */

/* Touch calibration defaults (overwritten at runtime by
 * XPT2046_SetCalibration once you've run a calibration pass for your
 * specific panel; the calibration UI itself is the app's job). */
#define DRV_TOUCH_RAW_X_MIN          262u
#define DRV_TOUCH_RAW_X_MAX          1872u
#define DRV_TOUCH_RAW_Y_MIN          230u
#define DRV_TOUCH_RAW_Y_MAX          1872u

#endif /* DRV_CONSTANTS_H_ */
