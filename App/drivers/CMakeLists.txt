cmake_minimum_required(VERSION 3.22)
project(tft_drivers C)

# -----------------------------------------------------------------------------
# Consumer contract
#
# This library does not know your CubeMX include paths, MCU defines, or where
# drv_constants.h lives. Before add_subdirectory()'ing this repo, define an
# INTERFACE target named `drv_platform` that carries:
#   - the include directory containing your app's drv_constants.h (App/)
#   - your CubeMX Core/Inc, Drivers/CMSIS, Drivers/STM32<family>_HAL_Driver
#     include directories
#   - your CMSIS-RTOS2 / FreeRTOS include directories (when DRV_USE_CMSIS_RTOS2)
#   - MCU compile definitions (e.g. STM32F446xx, USE_HAL_DRIVER)
#
#   add_library(drv_platform INTERFACE)
#   target_include_directories(drv_platform INTERFACE App/ ...)
#   target_compile_definitions(drv_platform INTERFACE STM32F446xx USE_HAL_DRIVER)
#   add_subdirectory(path/to/ILI9341-TFT-TouchGFX tft_drivers)
# -----------------------------------------------------------------------------

option(DRV_USE_CMSIS_RTOS2       "Use CMSIS-RTOS2 OSAL backend (else bare-metal)" ON)
option(DRV_PROVIDE_HAL_CALLBACKS "Compile default HAL callback glue"             ON)

set(DRV_SPI_TIMEOUT_MS           "100" CACHE STRING "Blocking SPI transfer timeout (ms)")
set(DRV_SPI_MAX_BUSES            "2"   CACHE STRING "Max concurrent drv_spi_bus_t objects")
set(ILI9341_MAX_LINE_PX          "320" CACHE STRING "Max ILI9341_FillScreen line width (px)")

add_library(tft_drivers STATIC
    src/drv_setup.c
    src/ili9341.c
    src/xpt2046.c
    src/port/drv_spi_stm32.c
    src/port/drv_gpio_stm32.c
)

if(DRV_USE_CMSIS_RTOS2)
    target_sources(tft_drivers PRIVATE src/os/drv_os_cmsis2.c)
else()
    target_sources(tft_drivers PRIVATE src/os/drv_os_baremetal.c)
endif()

if(DRV_PROVIDE_HAL_CALLBACKS)
    target_sources(tft_drivers PRIVATE src/drv_isr_glue.c)
endif()

target_include_directories(tft_drivers PUBLIC include)

target_compile_definitions(tft_drivers PUBLIC
    DRV_USE_CMSIS_RTOS2=$<BOOL:${DRV_USE_CMSIS_RTOS2}>
    DRV_PROVIDE_HAL_CALLBACKS=$<BOOL:${DRV_PROVIDE_HAL_CALLBACKS}>
    DRV_SPI_TIMEOUT_MS=${DRV_SPI_TIMEOUT_MS}
    DRV_SPI_MAX_BUSES=${DRV_SPI_MAX_BUSES}
    ILI9341_MAX_LINE_PX=${ILI9341_MAX_LINE_PX}
)

if(TARGET drv_platform)
    target_link_libraries(tft_drivers PRIVATE drv_platform)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_layering.cmake)
