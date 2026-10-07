#ifndef FLASH_SAFETY_H
#define FLASH_SAFETY_H

// App-wide flash_safe_execute() policy (strong override of pico_flash's weak
// get_flash_safety_helper(), see flash_safety.cpp) plus BTstack TLV bank
// diagnostics for GitHub issue #2 (empty paired-controllers list).

// Must be called right BEFORE core1 is launched (audio_init). Lets the helper
// distinguish "core1 was never started -> a direct IRQs-off flash write is
// safe" from "core1 is up but not yet a registered lockout victim -> refuse".
void flash_safety_note_core1_launch();

// UART diagnostic: print the BTstack TLV bank headers (valid/INVALID + epoch).
// Called around cyw43_arch_init() so a boot log shows whether the first-boot
// bank format actually landed in flash. `when` tags the log line.
void flash_safety_log_btstack_bank(const char *when);

#endif // FLASH_SAFETY_H
