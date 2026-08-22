# Wires the layering_check_script.cmake rules into both a build-time target
# and CTest. Included from the top-level CMakeLists.txt.

add_custom_target(layering_check
    COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_LIST_DIR}/layering_check_script.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking driver layering rules (no CubeMX leaks, HAL-free Layer 1 headers)"
    VERBATIM
)
add_dependencies(tft_drivers layering_check)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
endif()

add_test(NAME layering_check
    COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_LIST_DIR}/layering_check_script.cmake
)
