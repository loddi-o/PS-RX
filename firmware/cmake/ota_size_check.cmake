# OTA image-size guard (see the POST_BUILD hook in the top-level CMakeLists).
#
# The OTA design double-buffers in flash: the running image occupies
# [0, 2 MB) and the downloaded image is staged at [2 MB, config sector).
# Both constraints collapse to "the .bin must be <= LIMIT bytes", where LIMIT
# is min(staging offset, staging capacity) = 2080768 on the 4 MB boards.
# A build that outgrows this would produce a firmware whose OTA either can't
# stage (too big for the staging window) or whose SUCCESSOR can't be staged
# (image tail overlaps staging) -- fail loudly at build time instead.
#
# -DBIN=<path to ds5-bridge.bin> -DLIMIT=<max bytes>

if (NOT EXISTS "${BIN}")
    message(FATAL_ERROR "OTA size guard: ${BIN} not found (did pico_add_extra_outputs run?)")
endif ()
file(SIZE "${BIN}" _sz)
if (_sz GREATER ${LIMIT})
    message(FATAL_ERROR
        "OTA size guard FAILED: ${BIN} is ${_sz} bytes, limit ${LIMIT}. "
        "The image no longer fits below the 2 MB OTA staging offset. "
        "Either shrink the image or redesign the staging layout (src/ota.cpp).")
endif ()
math(EXPR _pct "100 * ${_sz} / ${LIMIT}")
message(STATUS "OTA size guard OK: ${_sz} / ${LIMIT} bytes (${_pct}%)")
