#include "drv/ili9341.h"
#include <string.h>

#ifndef ILI9341_MAX_INSTANCES
#define ILI9341_MAX_INSTANCES 1
#endif

#ifndef ILI9341_TIMEOUT_MS
#define ILI9341_TIMEOUT_MS 100u
#endif

struct ili9341 {
    bool in_use;
    drv_spi_bus_t *bus;
    drv_gpio_t cs, dc, reset;
    uint16_t hor_res, ver_res;
    uint8_t rotation;

    volatile bool flush_busy;
    ili9341_done_cb_t done_cb;
    void *done_user;

    /* FlushAsync chunking state, touched only under the bus lock (setup) or
     * from the tx-done ISR hook (continuation) — never both at once, because
     * the bus stays locked for the whole async transfer. */
    const uint8_t *chunk_ptr;
    uint32_t chunk_remaining;
};

static struct ili9341 s_instances[ILI9341_MAX_INSTANCES];

volatile ili9341_debug_t g_ili9341_debug;

static void ili_cmd(ili9341_t *d, uint8_t cmd)
{
    drv_gpio_write(d->dc, false);
    drv_gpio_write(d->cs, false);
    drv_spi_tx(d->bus, &cmd, 1, ILI9341_TIMEOUT_MS);
    drv_gpio_write(d->cs, true);
}

static void ili_data(ili9341_t *d, uint8_t data)
{
    drv_gpio_write(d->dc, true);
    drv_gpio_write(d->cs, false);
    drv_spi_tx(d->bus, &data, 1, ILI9341_TIMEOUT_MS);
    drv_gpio_write(d->cs, true);
}

static void ili_data16(ili9341_t *d, uint16_t v)
{
    uint8_t buf[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFFu) };
    drv_gpio_write(d->dc, true);
    drv_gpio_write(d->cs, false);
    drv_spi_tx(d->bus, buf, 2, ILI9341_TIMEOUT_MS);
    drv_gpio_write(d->cs, true);
}

static uint8_t madctl_for_rotation(uint8_t rot)
{
    switch (rot) {
        case 0:  return 0x48u; /* MX | BGR */
        case 1:  return 0x28u; /* MY | BGR */
        case 2:  return 0x88u; /* MX | MY | BGR */
        case 3:  /* fallthrough */
        default: return 0xE8u; /* MX | MY | MV | BGR (landscape) */
    }
}

static void ili_set_window(ili9341_t *d, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    ili_cmd(d, 0x2A);
    ili_data(d, (uint8_t)(x0 >> 8));
    ili_data(d, (uint8_t)(x0 & 0xFFu));
    ili_data(d, (uint8_t)(x1 >> 8));
    ili_data(d, (uint8_t)(x1 & 0xFFu));

    ili_cmd(d, 0x2B);
    ili_data(d, (uint8_t)(y0 >> 8));
    ili_data(d, (uint8_t)(y0 & 0xFFu));
    ili_data(d, (uint8_t)(y1 >> 8));
    ili_data(d, (uint8_t)(y1 & 0xFFu));

    ili_cmd(d, 0x2C);
}

static void ili_reset_pulse(ili9341_t *d)
{
    drv_gpio_write(d->reset, false);
    drv_delay_ms(20);
    drv_gpio_write(d->reset, true);
    drv_delay_ms(150);
}

static void ili_run_init_sequence(ili9341_t *d)
{
    ili_cmd(d, 0x01);
    drv_delay_ms(10);
    ili_cmd(d, 0x28);

    ili_cmd(d, 0xCF);
    ili_data(d, 0x00);
    ili_data(d, 0xC1);
    ili_data(d, 0x30);

    ili_cmd(d, 0xED);
    ili_data(d, 0x64);
    ili_data(d, 0x03);
    ili_data(d, 0x12);
    ili_data(d, 0x81);

    ili_cmd(d, 0xE8);
    ili_data(d, 0x85);
    ili_data(d, 0x00);
    ili_data(d, 0x78);

    ili_cmd(d, 0xCB);
    ili_data(d, 0x39);
    ili_data(d, 0x2C);
    ili_data(d, 0x00);
    ili_data(d, 0x34);
    ili_data(d, 0x02);

    ili_cmd(d, 0xF7);
    ili_data(d, 0x20);

    ili_cmd(d, 0xEA);
    ili_data(d, 0x00);
    ili_data(d, 0x00);

    ili_cmd(d, 0xC0);
    ili_data(d, 0x23);

    ili_cmd(d, 0xC1);
    ili_data(d, 0x10);

    ili_cmd(d, 0xC5);
    ili_data(d, 0x3e);
    ili_data(d, 0x28);

    ili_cmd(d, 0xC7);
    ili_data(d, 0x86);

    ili_cmd(d, 0x3A);
    ili_data(d, 0x55);

    ili_cmd(d, 0xB1);
    ili_data(d, 0x00);
    ili_data(d, 0x18);

    ili_cmd(d, 0xB6);
    ili_data(d, 0x08);
    ili_data(d, 0x82);
    ili_data(d, 0x27);

    ili_cmd(d, 0xF2);
    ili_data(d, 0x00);

    ili_cmd(d, 0x26);
    ili_data(d, 0x01);

    ili_cmd(d, 0x11);
    drv_delay_ms(120);
    ili_cmd(d, 0x29);
}

static void ili_tx_done_hook(void *ctx)
{
    ili9341_t *d = (ili9341_t *)ctx;

    if (d->chunk_remaining > 0u) {
        uint32_t next_len = d->chunk_remaining;
        uint32_t max_len = drv_spi_max_dma_len();
        if (next_len > max_len) {
            next_len = max_len;
        }
        const uint8_t *ptr = d->chunk_ptr;
        d->chunk_ptr += next_len;
        d->chunk_remaining -= next_len;

        if (drv_spi_tx_dma(d->bus, ptr, next_len) == DRV_OK) {
            return; /* more chunks in flight; CS/unlock happen after the last one */
        }
        /* DMA failed mid-transfer: fall through and finalize as best-effort. */
        d->chunk_remaining = 0u;
    }

    drv_gpio_write(d->cs, true);
    d->flush_busy = false;
    g_ili9341_debug.busy = 0;
    g_ili9341_debug.done_count++;
    /* Bus lock release from ISR context: valid for the mutex backends in this
     * tree (bare-metal critical section, and the host mock), matching the
     * plan's required async-flush contract. A CMSIS-RTOS2 mutex is not
     * generally ISR-safe; if that matters for your RTOS, prefer a GUI-side
     * policy that skips the flush while ILI9341_FlushBusy() is true rather
     * than contending the lock from an ISR. */
    drv_spi_bus_unlock(d->bus);

    if (d->done_cb != NULL) {
        d->done_cb(d->done_user);
    }
}

drv_spi_bus_t *ili9341_get_bus(ili9341_t *d)
{
    return (d != NULL) ? d->bus : NULL;
}

drv_status_t ILI9341_Init(ili9341_t **out, const ili9341_config_t *cfg)
{
    if (out == NULL || cfg == NULL || cfg->bus == NULL) {
        return DRV_ERR_PARAM;
    }
    if (cfg->hor_res > ILI9341_MAX_LINE_PX) {
        return DRV_ERR_PARAM;
    }

    ili9341_t *d = NULL;
    for (int i = 0; i < ILI9341_MAX_INSTANCES; i++) {
        if (!s_instances[i].in_use) {
            d = &s_instances[i];
            break;
        }
    }
    if (d == NULL) {
        return DRV_ERR_STATE;
    }

    memset(d, 0, sizeof(*d));
    d->in_use = true;
    d->bus = cfg->bus;
    d->cs = cfg->cs;
    d->dc = cfg->dc;
    d->reset = cfg->reset;
    d->hor_res = cfg->hor_res;
    d->ver_res = cfg->ver_res;
    d->rotation = cfg->rotation;

    drv_spi_bus_set_tx_done_hook(d->bus, ili_tx_done_hook, d);

    drv_status_t st = drv_spi_bus_lock(d->bus, DRV_OS_WAIT_FOREVER);
    if (st != DRV_OK) {
        d->in_use = false;
        return st;
    }

    ili_reset_pulse(d);
    ili_run_init_sequence(d);
    ili_cmd(d, 0x36);
    ili_data(d, madctl_for_rotation(d->rotation));

    drv_spi_bus_unlock(d->bus);

    *out = d;
    return DRV_OK;
}

drv_status_t ILI9341_SetRotation(ili9341_t *d, uint8_t rot)
{
    if (d == NULL || rot > 3u) {
        return DRV_ERR_PARAM;
    }
    drv_status_t st = drv_spi_bus_lock(d->bus, ILI9341_TIMEOUT_MS);
    if (st != DRV_OK) {
        return st;
    }
    ili_cmd(d, 0x36);
    ili_data(d, madctl_for_rotation(rot));
    d->rotation = rot;
    drv_spi_bus_unlock(d->bus);
    return DRV_OK;
}

drv_status_t ILI9341_FillScreen(ili9341_t *d, uint16_t rgb565)
{
    if (d == NULL) {
        return DRV_ERR_PARAM;
    }
    if (d->hor_res > ILI9341_MAX_LINE_PX) {
        return DRV_ERR_PARAM;
    }

    drv_status_t st = drv_spi_bus_lock(d->bus, ILI9341_TIMEOUT_MS);
    if (st != DRV_OK) {
        return st;
    }

    ili_set_window(d, 0, 0, (uint16_t)(d->hor_res - 1u), (uint16_t)(d->ver_res - 1u));

    uint8_t hi = (uint8_t)(rgb565 >> 8);
    uint8_t lo = (uint8_t)(rgb565 & 0xFFu);
    uint8_t line_buf[ILI9341_MAX_LINE_PX * 2u];
    for (uint16_t i = 0; i < d->hor_res; i++) {
        line_buf[i * 2u] = hi;
        line_buf[i * 2u + 1u] = lo;
    }

    drv_gpio_write(d->dc, true);
    drv_gpio_write(d->cs, false);
    drv_status_t tx_st = DRV_OK;
    for (uint16_t y = 0; y < d->ver_res; y++) {
        tx_st = drv_spi_tx(d->bus, line_buf, (uint32_t)d->hor_res * 2u, ILI9341_TIMEOUT_MS);
        if (tx_st != DRV_OK) {
            break;
        }
    }
    drv_gpio_write(d->cs, true);

    drv_spi_bus_unlock(d->bus);
    return tx_st;
}

drv_status_t ILI9341_DrawPixel(ili9341_t *d, uint16_t x, uint16_t y, uint16_t rgb565)
{
    if (d == NULL) {
        return DRV_ERR_PARAM;
    }
    drv_status_t st = drv_spi_bus_lock(d->bus, ILI9341_TIMEOUT_MS);
    if (st != DRV_OK) {
        return st;
    }
    ili_set_window(d, x, y, x, y);
    ili_data16(d, rgb565);
    drv_spi_bus_unlock(d->bus);
    return DRV_OK;
}

static drv_status_t spi_tx_chunked_blocking(drv_spi_bus_t *bus, const uint8_t *data, uint32_t len, uint32_t timeout_ms)
{
    uint32_t max_len = drv_spi_max_dma_len();
    uint32_t sent = 0;
    while (sent < len) {
        uint32_t chunk = (len - sent) > max_len ? max_len : (len - sent);
        drv_status_t st = drv_spi_tx(bus, data + sent, chunk, timeout_ms);
        if (st != DRV_OK) {
            return st;
        }
        sent += chunk;
    }
    return DRV_OK;
}

drv_status_t ILI9341_Flush(ili9341_t *d, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           const uint8_t *px, uint32_t len_bytes, uint32_t timeout_ms)
{
    if (d == NULL || px == NULL) {
        return DRV_ERR_PARAM;
    }
    drv_status_t st = drv_spi_bus_lock(d->bus, timeout_ms);
    if (st != DRV_OK) {
        return st;
    }

    ili_set_window(d, x0, y0, x1, y1);

    g_ili9341_debug.x = x0;
    g_ili9341_debug.y = y0;
    g_ili9341_debug.w = (uint16_t)(x1 - x0 + 1u);
    g_ili9341_debug.h = (uint16_t)(y1 - y0 + 1u);
    g_ili9341_debug.bytes = len_bytes;
    g_ili9341_debug.flush_count++;

    drv_gpio_write(d->dc, true);
    drv_gpio_write(d->cs, false);
    drv_status_t tx_st = spi_tx_chunked_blocking(d->bus, px, len_bytes, timeout_ms);
    drv_gpio_write(d->cs, true);

    drv_spi_bus_unlock(d->bus);
    return tx_st;
}

drv_status_t ILI9341_FlushAsync(ili9341_t *d, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                                const uint8_t *px, uint32_t len_bytes,
                                ili9341_done_cb_t cb, void *user)
{
    if (d == NULL || px == NULL) {
        return DRV_ERR_PARAM;
    }
    if (d->flush_busy) {
        return DRV_ERR_STATE;
    }

    drv_status_t st = drv_spi_bus_lock(d->bus, DRV_OS_WAIT_FOREVER);
    if (st != DRV_OK) {
        return st;
    }

    ili_set_window(d, x0, y0, x1, y1);

    d->done_cb = cb;
    d->done_user = user;

    g_ili9341_debug.x = x0;
    g_ili9341_debug.y = y0;
    g_ili9341_debug.w = (uint16_t)(x1 - x0 + 1u);
    g_ili9341_debug.h = (uint16_t)(y1 - y0 + 1u);
    g_ili9341_debug.bytes = len_bytes;
    g_ili9341_debug.flush_count++;

    uint32_t max_len = drv_spi_max_dma_len();
    uint32_t first_len = (len_bytes > max_len) ? max_len : len_bytes;
    d->chunk_ptr = px + first_len;
    d->chunk_remaining = len_bytes - first_len;

    drv_gpio_write(d->dc, true);
    drv_gpio_write(d->cs, false);

    drv_status_t dma_st = drv_spi_tx_dma(d->bus, px, first_len);
    if (dma_st != DRV_OK) {
        drv_gpio_write(d->cs, true);
        drv_spi_bus_unlock(d->bus);
        return DRV_ERR_HW;
    }

    d->flush_busy = true;
    g_ili9341_debug.busy = 1;
    return DRV_OK;
}

bool ILI9341_FlushBusy(ili9341_t *d)
{
    return (d != NULL) ? d->flush_busy : false;
}

drv_status_t ILI9341_WaitFlushDone(ili9341_t *d, uint32_t timeout_ms)
{
    if (d == NULL) {
        return DRV_ERR_PARAM;
    }
    uint32_t start = drv_tick_ms();
    while (d->flush_busy) {
        uint32_t remaining = DRV_OS_WAIT_FOREVER;
        if (timeout_ms != DRV_OS_WAIT_FOREVER) {
            uint32_t elapsed = drv_tick_ms() - start;
            if (elapsed >= timeout_ms) {
                return DRV_ERR_TIMEOUT;
            }
            remaining = timeout_ms - elapsed;
        }
        drv_status_t st = drv_spi_wait_tx_done(d->bus, remaining);
        if (st != DRV_OK) {
            return st;
        }
    }
    return DRV_OK;
}

void ILI9341_GetResolution(ili9341_t *d, uint16_t *hor_res, uint16_t *ver_res)
{
    if (d == NULL) {
        return;
    }
    if (hor_res) *hor_res = d->hor_res;
    if (ver_res) *ver_res = d->ver_res;
}

drv_status_t ILI9341_Sleep(ili9341_t *d, bool on)
{
    if (d == NULL) {
        return DRV_ERR_PARAM;
    }
    drv_status_t st = drv_spi_bus_lock(d->bus, ILI9341_TIMEOUT_MS);
    if (st != DRV_OK) {
        return st;
    }
    if (on) {
        ili_cmd(d, 0x10);
        drv_delay_ms(5);
    } else {
        ili_cmd(d, 0x11);
        drv_delay_ms(120);
    }
    drv_spi_bus_unlock(d->bus);
    return DRV_OK;
}
