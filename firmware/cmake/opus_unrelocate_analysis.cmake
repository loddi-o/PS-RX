# Un-relocate libopus's never-executed tonality-analysis objects (analysis +
# mlp_data) back to flash, AFTER the blanket .text->.time_critical.opus_text
# rename that moves the rest of libopus into RAM. This fork pins COMPLEXITY 0 /
# VBR off / fixed bitrate, so the analysis path never runs -- keeping it in flash
# reclaims ~21 KB of heap at zero audio risk.
#
# Run via `cmake -P` (not inline COMMANDs) because the archive member names are
# NOT stable across toolchains: this SDK names them analysis.c.obj / mlp_data.c.obj
# on Windows, but the Linux CI toolchain names them analysis.c.o / mlp_data.c.o.
# Hardcoding `.obj` made `ar x` fail with "no entry analysis.c.obj in archive" in
# CI. So we query the actual member names with `ar t` and match by basename,
# mirroring the suffix-tolerant approach in relocate_to_ram.cmake.
#
# Args (via -D): AR       (path to arm-none-eabi-ar)
#                OBJCOPY  (path to arm-none-eabi-objcopy)
#                ARCHIVE  (path to libopus.a)
#                WORKDIR  (scratch dir for extracted members)
foreach(_v AR OBJCOPY ARCHIVE WORKDIR)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "opus_unrelocate_analysis: missing required -D${_v}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WORKDIR}")

# List the archive members and pick the ones whose basename (sans .o/.obj) is
# exactly "analysis.c" or "mlp_data.c". Matching the basename avoids both the
# suffix mismatch and accidental hits on e.g. LPC_analysis_filter.c.obj.
execute_process(COMMAND "${AR}" t "${ARCHIVE}"
                OUTPUT_VARIABLE _members OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "opus_unrelocate_analysis: `ar t` failed (rc=${_rc}) on ${ARCHIVE}")
endif()
string(REPLACE "\n" ";" _members "${_members}")

set(_targets "")
foreach(_m ${_members})
    string(STRIP "${_m}" _m)
    string(REGEX REPLACE "\\.o(bj)?$" "" _base "${_m}")
    if(_base STREQUAL "analysis.c" OR _base STREQUAL "mlp_data.c")
        list(APPEND _targets "${_m}")
    endif()
endforeach()

if(NOT _targets)
    message(FATAL_ERROR "opus_unrelocate_analysis: no analysis.c / mlp_data.c member found in ${ARCHIVE}")
endif()

# Extract, reverse the section renames, and replace each member. All `ar`/objcopy
# member operations use the bare member name, so run them in WORKDIR.
foreach(_t ${_targets})
    execute_process(COMMAND "${AR}" x "${ARCHIVE}" "${_t}"
                    WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "opus_unrelocate_analysis: `ar x ${_t}` failed (rc=${_rc})")
    endif()
    execute_process(COMMAND "${OBJCOPY}"
                        --rename-section .time_critical.opus_text=.text
                        --rename-section .time_critical.opus_rodata=.rodata
                        --rename-section .time_critical.opus_strings=.rodata.str1.4
                        "${_t}"
                    WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "opus_unrelocate_analysis: objcopy failed (rc=${_rc}) on ${_t}")
    endif()
    execute_process(COMMAND "${AR}" r "${ARCHIVE}" "${_t}"
                    WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "opus_unrelocate_analysis: `ar r ${_t}` failed (rc=${_rc})")
    endif()
    file(REMOVE "${WORKDIR}/${_t}")
endforeach()
