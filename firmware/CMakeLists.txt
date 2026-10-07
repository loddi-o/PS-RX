# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Audio pipeline is real-time and crackles under -O0. Default to Release;
# override explicitly with -DCMAKE_BUILD_TYPE=Debug only when debugging.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if (WIN32)
    set(USERHOME $ENV{USERPROFILE})
else ()
    set(USERHOME $ENV{HOME})
endif ()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif ()
# ====================================================================================

option(PICO_W_BUILD "Build for Rasberry Pi Pico W (RP2040, audio is missing)" OFF)
option(WAVESHARE_RP2350B_PLUS_W_BUILD "Build for Waveshare RP2350B-Plus-W (USB-C, 16MB flash, RM2 wireless)" OFF)

if (PICO_W_BUILD AND WAVESHARE_RP2350B_PLUS_W_BUILD)
        message(FATAL_ERROR "PICO_W_BUILD and WAVESHARE_RP2350B_PLUS_W_BUILD are mutually exclusive")
endif ()

if (PICO_W_BUILD)
        set(PICO_BOARD pico_w CACHE STRING "Board type")
        set(PICO_RP2350 OFF CACHE BOOL "Not using RP2350")
elseif (WAVESHARE_RP2350B_PLUS_W_BUILD)
        set(PICO_BOARD waveshare_rp2350b_plus_w CACHE STRING "Board type")
        # The Waveshare board header is not in upstream pico-sdk; ship it here
        # and tell the SDK to search this directory when resolving PICO_BOARD.
        # It wires the RM2/CYW43 module to different RP2350 GPIOs than the Pico 2 W
        # (WL_REG_ON GP36, WL_DATA GP37, WL_CS GP38, WL_CLOCK GP39), so a stock
        # pico2_w build can't drive the radio on this board.
        list(APPEND PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/boards/headers")
else ()
        set(PICO_BOARD pico2_w CACHE STRING "Board type")
endif ()

message(STATUS "DS5-Linux-Bridge build target -- PICO_BOARD=${PICO_BOARD}")
message(STATUS "DS5-Linux-Bridge build target -- PICO_W_BUILD=${PICO_W_BUILD}")
message(STATUS "DS5-Linux-Bridge build target -- WAVESHARE_RP2350B_PLUS_W_BUILD=${WAVESHARE_RP2350B_PLUS_W_BUILD}")

# Diagnostics go over UART0 (GP0 TX, 115200 8N1), always available -- there is
# no USB-CDC serial. USB serial was removed because it perturbed the very USB
# timing it was meant to diagnose (and was unreliable). See README "Debugging".

# Config web UI + Wake-on-LAN over the onboard CYW43 radio in STA mode, joining
# the home WLAN by DHCP (onboarded via a captive portal on first boot). This is
# the ONLY config transport -- the old USB CDC-NCM web transport was removed so
# the dongle's USB face can be a pure DualSense (the optional USB wake keyboard
# is a runtime web-UI toggle now, off by default -- see ENABLE_WAKE_HID below).
# WOL wakes a PC in S4/S5 (when USB remote-wake is unsupported) by broadcasting
# a magic packet to its NIC. Needs NO extra hardware -- the radio is the same
# one BTstack already drives (the WiFi firmware blob is ALREADY in the flash
# image, linked by pico_cyw43_arch regardless of use). Sets CYW43_LWIP=1 so the
# SDK's cyw43_arch owns the lwIP netif/DHCP for WiFi. BT (Opus,
# latency-critical) and WiFi share one chip + one PIO-SPI bus -- see the
# audio-contention notes. Turning this OFF leaves NO config UI (settings keep
# their flash values); it exists for a zero-WiFi minimal build only.
option(ENABLE_WIFI_WOL "Serve the config page + send Wake-on-LAN over the onboard CYW43 Wi-Fi (STA mode)" ON)
# Over-the-air updates from GitHub Releases (src/ota.cpp). The dongle reboots
# into a dedicated OTA mode (BT/audio never initialised -> heap free for TLS,
# core1 never launched -> flash writes need no lockout), pulls the release
# .bin over HTTPS (pinned CA roots, ota_certs.h), stages it at +2 MB, verifies
# the release's SHA-256, then a RAM-resident stub copies staging over the live
# image and resets. Requires the WiFi transport, and a flash big enough to
# hold TWO images (staging sits at 2 MB): the 2 MB Pico W is excluded.
option(ENABLE_OTA "Firmware updates over WiFi from GitHub Releases" ON)
# Which GitHub repo's Releases feed OTA. Version discovery + downloads use the
# public web endpoints (releases/latest redirect + releases/download/...), so
# no API tokens and no rate-limit concerns.
set(OTA_REPO "kungaa/DS5-Linux-Bridge" CACHE STRING "GitHub owner/repo whose Releases serve OTA firmware")
if (ENABLE_OTA AND NOT ENABLE_WIFI_WOL)
    message(STATUS "ENABLE_OTA forced OFF: requires the WiFi transport (ENABLE_WIFI_WOL)")
    set(ENABLE_OTA OFF)
endif ()
if (ENABLE_OTA AND PICO_W_BUILD)
    message(STATUS "ENABLE_OTA forced OFF: Pico W's 2 MB flash cannot hold a staged second image")
    set(ENABLE_OTA OFF)
endif ()
# Concurrent controller slots. 1 = single-controller firmware (original
# behavior); 2-4 = the MULTI descriptor variant + per-slot BT connections
# become available (audio streams only while exactly one pad is connected --
# see src/tier.h). Bond capacity is NVM_NUM_LINK_KEYS (4) regardless.
set(MULTI_SLOT_COUNT 4 CACHE STRING "Maximum concurrent DualSense controllers (1-4)")
if (MULTI_SLOT_COUNT LESS 1 OR MULTI_SLOT_COUNT GREATER 4)
    message(FATAL_ERROR "MULTI_SLOT_COUNT must be 1-4 (got ${MULTI_SLOT_COUNT})")
endif ()
option(ENABLE_VERBOSE "Enable Log Verbose" OFF)
if(ENABLE_VERBOSE)
    set(ENABLE_VERBOSE_VALUE 1)
else()
    set(ENABLE_VERBOSE_VALUE 0)
endif ()
add_compile_definitions(ENABLE_VERBOSE=${ENABLE_VERBOSE_VALUE})
option(ENABLE_BATT_LED "Blink Pico LED when DualSense battery is low (<=10%)" ON)
if (ENABLE_BATT_LED)
        set(ENABLE_BATT_LED_VALUE 1)
else ()
        set(ENABLE_BATT_LED_VALUE 0)
endif ()
option(DISABLE_SPEAKER_PROC "Disable Speaker in controller" OFF)
if (DISABLE_SPEAKER_PROC)
        set(DISABLE_SPEAKER_PROC_VALUE 1)
else ()
        set(DISABLE_SPEAKER_PROC_VALUE 0)
endif ()
# Per-board clock / PIO config (PICO_W_BUILD declared near the top).
if (PICO_W_BUILD)
        set(CYW43_PIO_CLOCK_DIV_VALUE 3)
        set(SYS_CLOCK_KHZ_VALUE 200000)
        set(DISABLE_SPEAKER_PROC_VALUE 1)
else ()
        # Both RP2350 targets (Pico 2 W and Waveshare RP2350B-Plus-W) run a mild
        # 200 MHz overclock. Selective RAM relocation (relocate_to_ram) already
        # keeps the hot paths out of flash XIP, but Opus per-frame decode CPU still
        # steals cycles from the BT input-polling cadence on a single core; the
        # +33% core clock (150 -> 200 MHz) buys that headroom back. 200 MHz is RP
        # Pi's own well-tested point and runs at the SDK-default 1.10V core voltage
        # (no vreg bump -- see the overclock path in main.cpp). PIO div 3 keeps the
        # CYW43 SPI clock in spec at 200 MHz (div 2 was for the old 150 MHz clock).
        set(CYW43_PIO_CLOCK_DIV_VALUE 3)
        set(SYS_CLOCK_KHZ_VALUE 200000)
endif ()



# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ds5-bridge C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(ds5-bridge
        src/usb_descriptors.cpp
        src/main.cpp
        src/usb.cpp
        src/audio.cpp
        src/bt.cpp
        src/dse.cpp
        src/config.cpp
        src/flash_safety.cpp
        src/state_mgr.cpp
        src/tier.cpp
        src/ram_mem.c
)

# MULTI_SLOT_COUNT reaches every source (btstack_config.h / tusb_config.h /
# slots.h mirror a =1 fallback for safety). Global add_compile_definitions is
# NOT enough: BTstack + TinyUSB sources compile inside the ds5-bridge target
# and read it from btstack_config.h/tusb_config.h, which ARE per-target.
target_compile_definitions(ds5-bridge PRIVATE MULTI_SLOT_COUNT=${MULTI_SLOT_COUNT})

# RAM-resident memcpy/memset/memmove (src/ram_mem.c). Must be built with
# -fno-builtin and -fno-tree-loop-distribute-patterns so the compiler does not
# lower the word-copy loops back into calls to memcpy/memset (self-recursion).
set_source_files_properties(src/ram_mem.c PROPERTIES COMPILE_OPTIONS
        "-fno-builtin;-fno-tree-loop-distribute-patterns")

# Wi-Fi/WOL transport: config page + WOL over the onboard CYW43 in STA mode. No
# extra driver sources -- the cyw43 driver + WiFi firmware are already linked
# (pico_cyw43_arch, used for BT). We just add our STA/lwIP glue and ask the SDK
# to wire lwIP to the WiFi netif (CYW43_LWIP=1, set below where the other CYW43
# defs live). mDNS for ds5.local discovery. web_api.cpp is the httpd core
# (fs_open_custom / JSON API / POST handlers), kept in a separate file from the
# netif glue.
if (ENABLE_WIFI_WOL)
    target_sources(ds5-bridge PRIVATE
            src/web_api.cpp
            src/weblog.cpp
            src/wifi_net.cpp
            # Onboarding captive portal (AP mode): tiny DHCP + DNS servers so an
            # unprovisioned dongle can hand a phone a lease and redirect every
            # lookup to its own page. Vendored from pico-examples; only compiled
            # for the WiFi build (the AP path is the sole user). See lib/portal.
            lib/portal/dhcpserver.c
            lib/portal/dnsserver.c
    )
    target_include_directories(ds5-bridge PRIVATE
            lib/portal
    )
    target_compile_definitions(ds5-bridge PRIVATE
            ENABLE_WIFI_WOL
            # The web UI exists in this build (web_api.cpp + httpd).
            ENABLE_WEBUI
            # Regulatory country code for the CYW43 radio. The SDK default is
            # CYW43_COUNTRY_WORLDWIDE ('XX'), which makes the SoftAP BEACON but
            # REFUSE to admit stations (onboarding symptom: AP link up, but
            # cyw43_wifi_ap_get_stas stays 0 and clients loop DHCP forever). AP
            # mode needs a real regulatory domain. STA mode is unaffected (the
            # router's domain governs there), but this applies to both. Set to US
            # (permissive, widest 2.4GHz channel set).
            PICO_CYW43_ARCH_DEFAULT_COUNTRY_CODE=CYW43_COUNTRY\('U','S',0\)
    )
    target_link_libraries(ds5-bridge
            pico_unique_id
            pico_lwip_nosys
            pico_lwip_http
            pico_lwip_mdns
    )
endif ()

# GitHub OTA client (see the ENABLE_OTA option above for the design summary).
# Which release asset this board installs -- MUST match the stable-named .bin
# the release workflow attaches (see .github/workflows/release.yml). The
# board is the only differentiator: a verbose-debug user OTA-updates onto the
# standard image for their board.
if (ENABLE_OTA)
    if (WAVESHARE_RP2350B_PLUS_W_BUILD)
        set(OTA_ASSET_NAME "ds5-bridge-waveshare-ota.bin")
    else ()
        set(OTA_ASSET_NAME "ds5-bridge-pico2w-ota.bin")
    endif ()
    # lib/altcp_tls_patched/altcp_tls_mbedtls.c is the SDK's own (mbedTLS-3.x
    # patched) altcp TLS shim, vendored with one further fix: inner TCP window
    # credit is returned when ciphertext is CONSUMED, not when a TLS record
    # COMPLETES -- upstream's deferred credit hard-deadlocks any record larger
    # than the receive window (GitHub's CDN grows records past 16 KB a few
    # tens of KB into a download vs our ~4 KB window; HW-captured 2026-07-11).
    # Grep for PATCH(ds5-bridge in that file. Because of this we intentionally
    # do NOT link pico_lwip_mbedtls (which would compile the unpatched copy);
    # its other source + include dir are mirrored here.
    if (NOT DEFINED PICO_LWIP_PATH)
        set(PICO_LWIP_PATH ${PICO_SDK_PATH}/lib/lwip)
    endif ()
    target_sources(ds5-bridge PRIVATE
            src/ota.cpp
            lib/altcp_tls_patched/altcp_tls_mbedtls.c
            ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls_mem.c
    )
    target_include_directories(ds5-bridge PRIVATE
            ${PICO_LWIP_PATH}/src/apps/altcp_tls
    )
    # mbedTLS config: the SDK's pico_mbedtls_headers sets
    # MBEDTLS_CONFIG_FILE="pico_mbedtls_config.h", whose wrapper does
    # #include "mbedtls_config.h" -- resolved to src/mbedtls_config.h via this
    # target's include path (our minimal TLS-1.2-client config). Do NOT set
    # MBEDTLS_CONFIG_FILE here: it collides with the SDK's own define.
    target_compile_definitions(ds5-bridge PRIVATE
            ENABLE_OTA
            OTA_REPO=\"${OTA_REPO}\"
            OTA_ASSET_NAME=\"${OTA_ASSET_NAME}\"
            # Core0 stack 2 KB -> 4 KB: the mbedTLS 1.2 handshake (ECDSA
            # P-384 verify of the Sectigo chain in particular) runs deeper
            # than the SDK default stack. Costs the normal runtime 2 KB of
            # heap -- accounted against the ~27 KB cushion. OTA builds only.
            PICO_STACK_SIZE=0x1000
    )
    # NOTE: pico_lwip_mbedtls deliberately NOT linked -- see the vendored-shim
    # comment above.
    target_link_libraries(ds5-bridge
            pico_mbedtls
    )
endif ()

if (ENABLE_BATT_LED)
        target_sources(ds5-bridge PRIVATE src/battery_led.cpp)
endif ()


# Opus
#set(OPUS_FIXED_POINT ON)
add_subdirectory(lib/opus)

# Move libopus to RAM via .time_critical sections. libopus is built by
# add_subdirectory(lib/opus); the target `opus` is defined in the subdirectory's
# CMakeLists.txt, so we can't use POST_BUILD here. Instead use a stamp-file
# custom command + custom target that runs after opus is built and before
# ds5-bridge links. Renames .text/.rodata of libopus.a to .time_critical.* which
# the Pico SDK linker places in RAM (loaded from flash at boot, executed from RAM
# at runtime). ~220 KB relocated, ~5-6 ms boot-time impact for the copy.
#
# LOAD-BEARING: this is required for clean duplex audio. Opus's per-frame DSP AND
# its lookup tables (mdct/fft twiddles, CELT_PVQ_U_DATA, SILK codebooks) thrash the
# XIP cache when run from flash, causing audio stutter at stock 150 MHz. Upstream
# relocates the whole library for the same reason (commit 0adca8f). Do NOT drop
# this to free heap -- it trades clean audio for stutter. The web-config/audio RAM
# coexistence must be solved within the remaining heap, not by evicting Opus.
#
# EXCEPTION -- keep the tonality analyzer in flash (recover ~21 KB of RAM):
# analysis.c (run_analysis/tonality_analysis, ~16.5 KB) and its mlp_data.c table
# (~5 KB) are only used for auto-bitrate/complexity>=7 decisions. This firmware
# encodes at COMPLEXITY 0 with VBR OFF and a fixed bitrate (see audio.cpp), so the
# analysis path is NEVER executed at runtime -- it is pure dead weight in RAM.
# After the blanket relocation above, rename ONLY these two member objects' sections
# back to .text/.rodata so the linker leaves them in flash. Zero audio risk (never
# called => never XIP-fetched), ~21 KB of heap reclaimed. (Upstream relocated the
# whole library wholesale via one objcopy for submodule-pristine convenience, not
# because all of it is per-frame hot; this fork has heap pressure they did not.)
# NOTE: `ar x` extracts members into the process's working directory, and the
# subsequent objcopy/`ar r`/rm must all agree on that directory. add_custom_command
# does not guarantee a stable CWD (it differs between an in-source `build/` and an
# out-of-tree dir like the release CI's `build/standard`), which made objcopy fail
# with "analysis.c.obj: No such file". Pin an explicit scratch dir and reference the
# extracted members by absolute path so the sequence is build-dir-independent.
set(OPUS_RELOC_DIR ${CMAKE_BINARY_DIR}/opus_reloc)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/opus_ram_relocate.stamp
    # Blanket-relocate libopus code + lookup tables into RAM. String literals
    # (.rodata.str1.4, ~11 KB of error/assert messages) deliberately stay in
    # flash: they are never touched on the per-frame audio path, so they cost
    # zero XIP pressure -- and the heap needs the RAM (see the OOM note in
    # audio.cpp core1_entry).
    COMMAND ${CMAKE_OBJCOPY} --rename-section .text=.time_critical.opus_text --rename-section .rodata=.time_critical.opus_rodata $<TARGET_FILE:opus>
    # Then un-relocate the never-executed analysis path back to flash. This is a
    # cmake -P helper (not inline COMMANDs) because the archive member names are
    # not stable across toolchains (.c.obj here vs .c.o in CI), so it must query
    # `ar t` and match by basename. See cmake/opus_unrelocate_analysis.cmake.
    COMMAND ${CMAKE_COMMAND}
        -DAR=${CMAKE_AR}
        -DOBJCOPY=${CMAKE_OBJCOPY}
        -DARCHIVE=$<TARGET_FILE:opus>
        -DWORKDIR=${OPUS_RELOC_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/opus_unrelocate_analysis.cmake
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/opus_ram_relocate.stamp
    DEPENDS $<TARGET_FILE:opus> ${CMAKE_SOURCE_DIR}/cmake/opus_unrelocate_analysis.cmake
    COMMENT "Relocating libopus to RAM (tonality analyzer kept in flash)"
    VERBATIM
)
add_custom_target(opus_ram_relocate ALL DEPENDS ${CMAKE_BINARY_DIR}/opus_ram_relocate.stamp)
add_dependencies(ds5-bridge opus_ram_relocate)

target_compile_options(ds5-bridge PRIVATE -fsigned-char)

# Flash XIP timing. boot2 (w25q080) applies PICO_FLASH_SPI_CLKDIV; div 2 gives a
# safe flash SPI clock at our stock 150 MHz system clock (a higher overclock
# would need div 4). On RP2350 boot2 only runs when PICO_EMBED_XIP_SETUP=1.
if (${SYS_CLOCK_KHZ_VALUE} GREATER 266000)
        set(PICO_FLASH_SPI_CLKDIV 4)
else ()
        set(PICO_FLASH_SPI_CLKDIV 2)
endif ()
target_compile_definitions(bs2_default PRIVATE PICO_FLASH_SPI_CLKDIV=${PICO_FLASH_SPI_CLKDIV})

if (WAVESHARE_RP2350B_PLUS_W_BUILD)
        # The Waveshare board ships a Puya PY25Q128HA flash (not the schematic's
        # Winbond). Without forcing boot2 to run, the bootrom auto-detects from
        # SFDP and leaves XIP at CLKDIV=3 + Fast Read Dual I/O, which exceeds
        # Puya's dual-mode spec and intermittently corrupts XIP reads (choppy
        # audio). PICO_EMBED_XIP_SETUP=1 makes boot2_w25q080 actually run and
        # apply our PICO_FLASH_SPI_CLKDIV above + Fast Read Quad I/O.
        target_compile_definitions(ds5-bridge PRIVATE PICO_EMBED_XIP_SETUP=1)
endif ()

# CYW43_LWIP: the WiFi transport needs the SDK's cyw43_arch to own an lwIP netif
# for the radio (=1). Every other build keeps the radio BT-only and lets our code
# own lwIP alone -- or no lwIP at all -- so =0.
if (ENABLE_WIFI_WOL)
    set(CYW43_LWIP_VALUE 1)
else ()
    set(CYW43_LWIP_VALUE 0)
endif ()

target_compile_definitions(ds5-bridge PRIVATE
        CYW43_LWIP=${CYW43_LWIP_VALUE}
        # 0 (not 1): flash_safe_execute() must really park core1 while flash is
        # erased/programmed (config_save). With =1 the lockout is skipped and the
        # flash op races the audio core -> buzzing / crash when saving during
        # playback. core1_entry registers via flash_safe_execute_core_init().
        PICO_FLASH_ASSUME_CORE1_SAFE=0
        CYW43_PIO_CLOCK_DIV_INT=${CYW43_PIO_CLOCK_DIV_VALUE}
        ENABLE_VERBOSE=${ENABLE_VERBOSE_VALUE}
        ENABLE_BATT_LED=${ENABLE_BATT_LED_VALUE}
        SYS_CLOCK_KHZ=${SYS_CLOCK_KHZ_VALUE}
        DISABLE_SPEAKER_PROC=${DISABLE_SPEAKER_PROC_VALUE}
)

option(ENABLE_WAKE_HID "Wake the host when the controller connects / PS button is pressed" ON)
if(ENABLE_WAKE_HID)
    target_compile_definitions(ds5-bridge PRIVATE
            ENABLE_WAKE_HID
    )
    target_sources(ds5-bridge PRIVATE
            src/wake.cpp
    )
    # ENABLE_WAKE_HID is the wake DECISION machinery: the dynamic descriptor +
    # host-suspend tracking (MINIMAL/FULL variant swap, usb_host_suspended(),
    # the BT-event wake triggers, the DualSense power-off-on-suspend debounce).
    # The wake ACTIONS layer on top of it:
    #   - WOL over WiFi (ENABLE_WIFI_WOL above) -- covers S4/S5, no USB footprint.
    #   - USB boot keyboard -- an F15 keystroke + USB remote-wakeup, covers S3
    #     on boards without WOL. A RUNTIME web-UI toggle now (Wake section ->
    #     Config_body.wake_kbd_enabled), not a compile option: default OFF, so
    #     the default USB face is a PURE DualSense (audio + gamepad, nothing
    #     else -- the extra keyboard interface on Sony's VID/PID is what some
    #     anticheats treat as suspicious).
    #   - Companion-Pico link (ENABLE_WAKE_LINK) -- scaffolding for a second,
    #     separate Pico that enumerates as a plain boot keyboard on its own USB
    #     port/VID:PID and types the wake keystroke when this dongle signals it
    #     over a GPIO line. Keeps this dongle's enumeration pure while still
    #     having USB-HID wake. The companion firmware is future work; this side
    #     only emits the signal (see src/wake_link.h).
    option(ENABLE_WAKE_LINK "Signal a companion wake-keyboard Pico over GPIO on host-wake (scaffolding)" OFF)
    if(ENABLE_WAKE_LINK)
        target_compile_definitions(ds5-bridge PRIVATE ENABLE_WAKE_LINK)
        target_sources(ds5-bridge PRIVATE src/wake_link.cpp)
    endif()
else()
    # Multi-slot rides on the dynamic-descriptor machinery (the MULTI variant
    # + the mount-gated swap orchestrator live behind ENABLE_WAKE_HID). A
    # non-wake build is single-controller only.
    if (MULTI_SLOT_COUNT GREATER 1)
        message(FATAL_ERROR "MULTI_SLOT_COUNT>1 requires ENABLE_WAKE_HID=ON "
                "(the MULTI descriptor variant uses the dynamic-descriptor machinery); "
                "build with -DMULTI_SLOT_COUNT=1 for a non-wake firmware")
    endif ()
endif()

option(WAKE_DEBUG "Print wake-FSM transitions to UART (developer use)" OFF)
if(WAKE_DEBUG)
    target_compile_definitions(ds5-bridge PRIVATE
            WAKE_DEBUG
    )
endif()

set(VERSION "dev" CACHE STRING "Program version string")

pico_set_program_name(ds5-bridge "ds5-bridge")
pico_set_program_version(ds5-bridge "${VERSION}")

# Diagnostics go over UART0 only (GP0 TX, 115200 8N1). No USB stdio: USB-CDC
# serial perturbed the USB audio timing it was meant to diagnose.
pico_enable_stdio_uart(ds5-bridge 1)
pico_enable_stdio_usb(ds5-bridge 0)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add any user requested libraries
target_link_libraries(ds5-bridge
        pico_stdlib
        pico_multicore
        hardware_watchdog
        hardware_timer
        hardware_flash
        pico_btstack_classic
        pico_cyw43_arch_poll
        pico_btstack_cyw43
        tinyusb_device
        tinyusb_board
        opus
)

pico_add_extra_outputs(ds5-bridge)

# OTA image-size guard: the OTA scheme stages the downloaded image at flash
# offset 2 MB and needs the RUNNING image to end below that (and the staged
# copy to fit between 2 MB and the config sector at -4 sectors, 2080768 B on
# the 4 MB boards). Growing past either limit must fail the BUILD, not brick
# a user: enforce the tighter bound (2080768) on the produced .bin here.
# Registered after pico_add_extra_outputs so the .bin exists when it runs.
if (ENABLE_OTA)
    add_custom_command(TARGET ds5-bridge POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DBIN=$<TARGET_FILE_DIR:ds5-bridge>/ds5-bridge.bin
            -DLIMIT=2080768
            -P ${CMAKE_SOURCE_DIR}/cmake/ota_size_check.cmake
        COMMENT "OTA size guard: image must fit below the 2 MB staging offset"
        VERBATIM)
endif ()

# --- Selective RAM relocation ------------------------------------------------
# Instead of relocating the whole image into RAM (the old memmap_custom.ld
# approach, which needed the overclock to absorb the resulting pressure), keep
# .text in flash (stock linker script) and relocate only the specific hot
# functions that thrash the XIP cache. relocate_to_ram() renames the chosen
# .text.<fn> sections to .time_critical.<name> on the compiled objects in a
# PRE_LINK step; the Pico SDK linker places .time_critical.* in RAM (LMA flash,
# VMA RAM). The helper locates each object by its stable SDK-relative path
# SUFFIX. RENAMES are |-separated old=new pairs; objcopy skips absent sections,
# so this is idempotent across incremental relinks.
function(relocate_to_ram _suffix _renames _comment)
    add_custom_command(TARGET ds5-bridge PRE_LINK
        COMMAND ${CMAKE_COMMAND}
            "-DOBJROOT=${CMAKE_BINARY_DIR}/CMakeFiles/ds5-bridge.dir"
            "-DOBJCOPY=${CMAKE_OBJCOPY}"
            "-DSUFFIX=${_suffix}"
            "-DRENAMES=${_renames}"
            -P "${CMAKE_SOURCE_DIR}/cmake/relocate_to_ram.cmake"
        COMMENT "${_comment}"
        VERBATIM)
endfunction()

# Tier-1: per-packet BT receive + USB FIFO data path
relocate_to_ram(lib/btstack/src/hci.c.o
    ".text.packet_handler=.time_critical.bt_packet_handler"
    "relocate: hci.c packet_handler -> RAM")
relocate_to_ram(lib/btstack/src/l2cap.c.o
    ".text.l2cap_acl_handler=.time_critical.l2cap_acl_handler"
    "relocate: l2cap.c l2cap_acl_handler -> RAM")
relocate_to_ram(lib/tinyusb/src/common/tusb_fifo.c.o
    ".text.tu_fifo_read=.time_critical.tu_fifo_read|.text.tu_fifo_read_n=.time_critical.tu_fifo_read_n|.text.tu_fifo_write=.time_critical.tu_fifo_write|.text.tu_fifo_write_n=.time_critical.tu_fifo_write_n|.text.tu_fifo_count=.time_critical.tu_fifo_count"
    "relocate: tusb_fifo.c data path -> RAM")
relocate_to_ram(src/rp2_common/pico_cyw43_driver/cyw43_bus_pio_spi.c.o
    ".text.cyw43_spi_transfer=.time_critical.cyw43_spi_transfer|.text.cyw43_read_bytes=.time_critical.cyw43_read_bytes|.text.cyw43_write_bytes=.time_critical.cyw43_write_bytes|.text.cyw43_read_reg_u32=.time_critical.cyw43_read_reg_u32|.text.cyw43_read_reg_u16=.time_critical.cyw43_read_reg_u16|.text.cyw43_read_reg_u8=.time_critical.cyw43_read_reg_u8|.text.cyw43_write_reg_u32=.time_critical.cyw43_write_reg_u32|.text.cyw43_write_reg_u16=.time_critical.cyw43_write_reg_u16|.text.cyw43_write_reg_u8=.time_critical.cyw43_write_reg_u8|.text.read_reg_u32_swap=.time_critical.read_reg_u32_swap|.text.write_reg_u32_swap=.time_critical.write_reg_u32_swap|.text.ns_delay.constprop.0=.time_critical.cyw43_ns_delay"
    "relocate: cyw43 PIO-SPI transport -> RAM")
relocate_to_ram(lib/cyw43-driver/src/cyw43_ll.c.o
    ".text.cyw43_ll_sdpcm_poll_device=.time_critical.cyw43_ll_sdpcm_poll_device|.text.cyw43_ll_process_packets=.time_critical.cyw43_ll_process_packets|.text.cyw43_sdpcm_send_common=.time_critical.cyw43_sdpcm_send_common|.text.cyw43_do_ioctl.part.0=.time_critical.cyw43_do_ioctl|.text.cyw43_ll_bt_has_work=.time_critical.cyw43_ll_bt_has_work|.text.cyw43_ll_parse_async_event=.time_critical.cyw43_ll_parse_async_event"
    "relocate: cyw43 SDPCM pump -> RAM")
relocate_to_ram(lib/cyw43-driver/src/cyw43_ctrl.c.o
    ".text.cyw43_poll_func=.time_critical.cyw43_poll_func|.text.cyw43_bluetooth_hci_read=.time_critical.cyw43_bluetooth_hci_read|.text.cyw43_bluetooth_hci_write=.time_critical.cyw43_bluetooth_hci_write"
    "relocate: cyw43 poll + BT HCI bus -> RAM")

# Tier-2: run-loop TX pumps + per-transfer submit paths.
# state_update is intentionally NOT moved: it runs on the host->controller
# HID-OUT (rumble/LED) path, not the audio frame path, so it stays in flash.
relocate_to_ram(lib/btstack/src/hci.c.o
    ".text.hci_run=.time_critical.hci_run|.text.hci_send_acl_packet_fragments=.time_critical.hci_send_acl_packet_fragments|.text.hci_send_acl_packet_buffer=.time_critical.hci_send_acl_packet_buffer"
    "relocate: hci.c TX pump + ACL out -> RAM")
relocate_to_ram(lib/btstack/src/l2cap.c.o
    ".text.l2cap_run=.time_critical.l2cap_run|.text.l2cap_send=.time_critical.l2cap_send|.text.l2cap_send_prepared=.time_critical.l2cap_send_prepared|.text.l2cap_notify_channel_can_send=.time_critical.l2cap_notify_channel_can_send"
    "relocate: l2cap.c TX pump + sends -> RAM")
relocate_to_ram(lib/cyw43-driver/src/cyw43_ll.c.o
    ".text.cyw43_ll_write_backplane_mem=.time_critical.cyw43_ll_write_backplane_mem|.text.cyw43_ll_read_backplane_mem=.time_critical.cyw43_ll_read_backplane_mem|.text.cyw43_write_backplane=.time_critical.cyw43_write_backplane|.text.cyw43_read_backplane.constprop.0=.time_critical.cyw43_read_backplane"
    "relocate: cyw43 backplane accessors -> RAM")
relocate_to_ram(lib/tinyusb/src/device/usbd.c.o
    ".text.tud_task_ext=.time_critical.tud_task_ext|.text.usbd_edpt_xfer=.time_critical.usbd_edpt_xfer|.text.usbd_edpt_xfer_fifo=.time_critical.usbd_edpt_xfer_fifo"
    "relocate: usbd.c task pump + xfer -> RAM")
relocate_to_ram(lib/tinyusb/src/portable/raspberrypi/rp2040/dcd_rp2040.c.o
    ".text.dcd_edpt_xfer=.time_critical.dcd_edpt_xfer|.text.dcd_edpt_iso_activate=.time_critical.dcd_edpt_iso_activate"
    "relocate: dcd_rp2040.c submit + iso -> RAM")
relocate_to_ram(lib/tinyusb/src/class/audio/audio_device.c.o
    ".text.tud_audio_n_read=.time_critical.tud_audio_n_read|.text.tud_audio_n_write=.time_critical.tud_audio_n_write|.text.audiod_calc_tx_packet_sz.isra.0=.time_critical.audiod_calc_tx_packet_sz|.text.audiod_xfer_isr=.time_critical.audiod_xfer_isr"
    "relocate: audio_device.c data path -> RAM")

# Tier-3: input-report path tail (controller -> host).
# queue_try_* also back the audio inter-core FIFOs, so this helps both paths.
relocate_to_ram(lib/tinyusb/src/class/hid/hid_device.c.o
    ".text.tud_hid_n_report=.time_critical.tud_hid_n_report|.text.tud_hid_n_ready=.time_critical.tud_hid_n_ready|.text.hidd_xfer_cb=.time_critical.hidd_xfer_cb"
    "relocate: hid_device.c report path -> RAM")
relocate_to_ram(src/common/pico_util/queue.c.o
    ".text.queue_try_add=.time_critical.queue_try_add|.text.queue_try_remove=.time_critical.queue_try_remove"
    "relocate: pico_util queue add/remove -> RAM")
relocate_to_ram(src/rp2_common/pico_cyw43_arch/cyw43_arch.c.o
    ".text.cyw43_arch_poll=.time_critical.cyw43_arch_poll"
    "relocate: cyw43_arch_poll -> RAM")
relocate_to_ram(lib/btstack/src/btstack_run_loop.c.o
    ".text.btstack_run_loop_base_poll_data_sources=.time_critical.btstack_run_loop_base_poll_data_sources|.text.btstack_run_loop_poll_data_sources_from_irq=.time_critical.btstack_run_loop_poll_data_sources_from_irq"
    "relocate: btstack run-loop poll -> RAM")
