# Hardware Smoke Checklist (Display + Touch)

Use this checklist after flashing firmware to validate the full integrated
display subsystem on real hardware. This is the final gate for Phase 8 of
`REWORK_PLAN.md` and must be run by a human with the board in hand — the
host unit tests in `tests/host/` catch driver-level regressions but cannot
observe real SPI timing, DMA completion, or panel/touch behavior.

## Preconditions

- Firmware built from the current branch, targeting `Profiles/<board>/`
  (default `Profiles/board_1/`).
- `App/drv_constants.h` matches the board actually connected (pins, SPI
  handles, rotation, calibration defaults).
- Display and touch hardware wired per `App/drv_constants.h`.

## Checks

1. Boot and initialization
   - Device boots without HardFault.
   - No repeated init/reset loop is observed.

2. Display output
   - Backlight/display powers on.
   - TouchGFX content renders with correct orientation for `DRV_DISP_ROTATION`.
   - No persistent tearing or corrupted frame regions (checks the DMA
     chunking and BSY-clear fix from Phase 2 under real SPI timing).

3. Touch input
   - Touch press is detected on-screen.
   - Drag/move actions track as expected.
   - Corner taps map to expected UI locations for the configured rotation
     (validates the rotation-aware `XPT2046_ReadPoint` mapping from Phase 2 —
     rotations other than the legacy-validated one were derived
     analytically and need real-hardware confirmation here).

4. Calibration behavior
   - Run your app's calibration entry point (raw corner capture).
   - Verify improved touch accuracy after calibration.
   - Verify the calibration values persist correctly via
     `XPT2046_SetCalibration`/`XPT2046_GetCalibration` for the session.

5. Stability
   - Leave GUI running for 10+ minutes.
   - Verify no freeze, watchdog reset, or display DMA deadlock.

6. Concurrent touch/display stress (new — validates Phase 1's bus-lock
   discipline on a shared SPI bus)
   - Configure `DRV_TOUCH_SPI_HANDLE == DRV_DISP_SPI_HANDLE` (shared bus).
   - Run a second task that calls `XPT2046_ReadRaw`/`ReadPoint` in a tight
     loop while the GUI animates continuously, for 5 minutes.
   - Verify no visual corruption, no touch misreads, no deadlock — the bus
     lock must serialize display flushes and touch reads without either
     starving the other.

7. DMA soak under sustained redraw (validates the async flush path)
   - Force a full-screen redraw every frame (no partial-region skipping).
   - Run the same animation/stress workload as #6 for 5+ minutes.
   - Verify no stalled transfer leaves `ILI9341_FlushBusy()` stuck true
     (would show up as skipped/garbled frames) and no permanent frame
     freeze on a timeout.

## Result

- Pass if all checks complete without failure.
- Fail if any check is unstable or incorrect; capture logs/UART traces and
  the `Profiles/<board>` used.
