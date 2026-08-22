/**
 * TouchGFXDisplayDriver.cpp
 *
 * Board glue between TouchGFX's Partial Framebuffer strategy and the
 * tft_drivers ILI9341 SPI driver. Hand-written (not TouchGFX-generated);
 * implements the two extern "C" hooks TouchGFXGeneratedHAL.cpp declares.
 *
 * Completion handling is deferred to a small high-priority "pump" task:
 * ILI9341_FlushAsync's done callback fires in ISR context, but chaining the
 * next block means calling ILI9341_FlushAsync again, which acquires the SPI
 * bus lock with an infinite timeout — illegal from an ISR under CMSIS-RTOS2
 * (nonzero-timeout osSemaphoreAcquire returns osErrorParameter there). The
 * ISR therefore only releases a semaphore; the pump task runs
 * DisplayDriver_TransferCompleteCallback() in task context.
 */

#include <stdint.h>

#include "cmsis_os2.h"

extern "C" {

#include "drv/drv_setup.h"

/* Defined in TouchGFX/target/generated/TouchGFXGeneratedHAL.cpp. */
void DisplayDriver_TransferCompleteCallback(void);

/* True from transmitBlock() until the pump task is about to run the
 * transfer-complete callback. Deliberately not ILI9341_FlushBusy(): that
 * clears in the DMA ISR, which would let the TouchGFX task submit a new
 * block before the pump has processed the previous completion. The pump
 * task must run at higher priority than the TouchGFX task so the two never
 * interleave inside the FrameBufferAllocator. */
static volatile bool s_transfer_active = false;

static osSemaphoreId_t s_pump_sem = 0;

static void flush_done_cb(void *user) /* ISR context */
{
    (void)user;
    osSemaphoreRelease(s_pump_sem);
}

static void pump_task(void *arg)
{
    (void)arg;
    for (;;) {
        osSemaphoreAcquire(s_pump_sem, osWaitForever);
        s_transfer_active = false;
        /* Frees the transferred block and submits the next ready one (calls
         * back into touchgfxDisplayDriverTransmitBlock, task context). */
        DisplayDriver_TransferCompleteCallback();
    }
}

int touchgfxDisplayDriverTransmitActive(void)
{
    return s_transfer_active ? 1 : 0;
}

void touchgfxDisplayDriverTransmitBlock(const uint8_t *pixels, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    ili9341_t *d = DRV_GetDisplay();
    const uint32_t len = (uint32_t)w * h * 2u;

    /* TouchGFX's LCD16bpp renders native-endian RGB565 (low byte first in
     * memory), but the panel expects each pixel's high byte first over SPI.
     * ILI9341_Flush{,Async}() transmit the buffer byte-for-byte with no
     * reordering, so the swap has to happen here. Safe to
     * do in place: `pixels` points into a writable block-allocator buffer
     * that TouchGFX won't touch again until the transfer-complete callback
     * frees it. */
    uint8_t *rw_pixels = (uint8_t *)pixels;
    for (uint32_t i = 0; i + 1u < len; i += 2u) {
        uint8_t t = rw_pixels[i];
        rw_pixels[i] = rw_pixels[i + 1u];
        rw_pixels[i + 1u] = t;
    }

    s_transfer_active = true;
    if (ILI9341_FlushAsync(d, x, y, x + w - 1u, y + h - 1u, pixels, len,
                           flush_done_cb, (void *)0) != DRV_OK) {
        /* Fallback: blocking flush, then release the block synchronously. */
        (void)ILI9341_Flush(d, x, y, x + w - 1u, y + h - 1u, pixels, len,
                            DRV_SPI_TIMEOUT_MS);
        s_transfer_active = false;
        DisplayDriver_TransferCompleteCallback();
    }
}

/* Called once from the TouchGFX task (App/app_main.c) after DRV_Setup(),
 * before MX_TouchGFX_Process(). */
void TouchGFXDisplayDriver_Init(void)
{
    static const osSemaphoreAttr_t sem_attr = { .name = "tgfxPumpSem" };
    static const osThreadAttr_t task_attr = {
        .name = "tgfxPump",
        .stack_size = 1024,
        .priority = (osPriority_t)osPriorityAboveNormal,
    };
    s_pump_sem = osSemaphoreNew(1u, 0u, &sem_attr);
    (void)osThreadNew(pump_task, (void *)0, &task_attr);
}

} /* extern "C" */
