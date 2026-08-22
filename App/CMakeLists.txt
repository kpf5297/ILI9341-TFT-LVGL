# App/ — application code + the tft_drivers wiring. Never regenerated
# by CubeMX; everything CubeMX owns lives under Profiles/${PROFILE}/.

set(TFT_DRIVERS_ROOT ${CMAKE_CURRENT_LIST_DIR}/drivers)
set(PROFILE_ROOT ${CMAKE_CURRENT_LIST_DIR}/../Profiles/${PROFILE})

# ---------------------------------------------------------------------------
# drv_platform: the contract tft_drivers' CMakeLists.txt asks every consumer
# to provide (include dirs / defines for the CubeMX + CMSIS-RTOS2 headers
# App/drv_constants.h and the driver ports need, but the library itself
# must not know about).
# ---------------------------------------------------------------------------
add_library(drv_platform INTERFACE)
target_include_directories(drv_platform INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PROFILE_ROOT}/Core/Inc
    ${PROFILE_ROOT}/Drivers/CMSIS/Include
    ${PROFILE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${PROFILE_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${PROFILE_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${PROFILE_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${PROFILE_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    ${PROFILE_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
)
target_compile_definitions(drv_platform INTERFACE
    STM32F446xx
    USE_HAL_DRIVER
)

# ---------------------------------------------------------------------------
# tft_drivers (vendored under App/drivers/)
# ---------------------------------------------------------------------------
# This project's UI is TouchGFX (generated under Profiles/${PROFILE}/TouchGFX,
# built by Profiles/${PROFILE}/cmake/touchgfx).
set(DRV_USE_CMSIS_RTOS2 ON CACHE BOOL "" FORCE)
add_subdirectory(${TFT_DRIVERS_ROOT} tft_drivers)

# ---------------------------------------------------------------------------
# App sources
# ---------------------------------------------------------------------------
add_library(app STATIC
    app_main.c
    system.c
    clock_protocol.c
)
target_include_directories(app PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(app PUBLIC tft_drivers drv_platform)
