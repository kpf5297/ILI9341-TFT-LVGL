// TouchController.h
#ifndef TOUCH_CONTROLLER_H_
#define TOUCH_CONTROLLER_H_

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the XPT2046 driver and register it as an LVGL pointer device.
 * Call this once, before you call touch_calibrate() or start your main loop.
 */
void TouchController_Init(void);

/**
 * Run a 4-point on-screen calibration.
 * This will block, prompting the user to tap the four crosses in sequence.
 * After it returns, the touch → pixel mapping is updated.
 */
void touch_calibrate(void);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_CONTROLLER_H_ */
