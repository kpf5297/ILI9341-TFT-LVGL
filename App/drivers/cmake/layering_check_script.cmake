# Standalone script (run via `cmake -P`) that enforces the Layer 0/1/2
# include-direction rules from REWORK_PLAN.md section 3.3 and the Phase 1/4
# acceptance criteria. Invoked both as a build-time custom target and as a
# CTest test by cmake/check_layering.cmake.

if(NOT DEFINED ROOT)
    message(FATAL_ERROR "layering_check_script.cmake requires -DROOT=<repo root>")
endif()

set(_fail 0)

function(_grep_files_forbidden files pattern label)
    foreach(f ${files})
        if(NOT EXISTS "${f}")
            continue()
        endif()
        file(STRINGS "${f}" _hits REGEX "${pattern}")
        if(_hits)
            message(WARNING "layering_check: forbidden ${label} in ${f}: ${_hits}")
            set(_fail 1 PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

function(_grep_dirs_forbidden dirs pattern label)
    set(_files "")
    foreach(dir ${dirs})
        if(EXISTS "${dir}")
            file(GLOB_RECURSE _dir_files "${dir}/*.c" "${dir}/*.h")
            list(APPEND _files ${_dir_files})
        endif()
    endforeach()
    _grep_files_forbidden("${_files}" "${pattern}" "${label}")
    if(_fail)
        set(_fail 1 PARENT_SCOPE)
    endif()
endfunction()

# No driver file may include CubeMX-generated headers directly.
_grep_dirs_forbidden("${ROOT}/include;${ROOT}/src;${ROOT}/adapters" "\"main\\.h\"|\"spi\\.h\"|\"gpio\\.h\"" "CubeMX include")

# Layer 1 public headers must be HAL-free.
_grep_files_forbidden("${ROOT}/include/drv/ili9341.h;${ROOT}/include/drv/xpt2046.h" "HAL_" "HAL_ symbol reference")

if(_fail)
    message(FATAL_ERROR "layering_check FAILED — see warnings above")
else()
    message(STATUS "layering_check passed")
endif()
