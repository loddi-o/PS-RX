// https://github.com/rafaelvaloto/Pico_W-Dualsense/blob/main/btstack_config.h

// c
#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

#ifndef ENABLE_CLASSIC
#define ENABLE_CLASSIC
#endif


// CYW43 HCI Transport requires pre-buffer space for packet header

// Concurrent controller support: connection/channel/buffer caps scale with
// MULTI_SLOT_COUNT (CMake option; slots.h can't be included here because
// BTstack's C sources compile this header too, so mirror its fallback).
#ifndef MULTI_SLOT_COUNT
#define MULTI_SLOT_COUNT 1
#endif

// Se estiver 1 ou 2, o 0x31 do DualSense causa estouro (per link).
// Each ACL buffer is ~1 KB of static RAM; 2 per link + 2 shared slack keeps
// the pool honest at 4 slots without eating the heap the opus states need.
// (At MULTI_SLOT_COUNT=1 this is the original value 4.)
#define MAX_NR_HCI_ACL_PACKETS (2 * MULTI_SLOT_COUNT + 2)

#define MAX_NR_HCI_CONNECTIONS MULTI_SLOT_COUNT
#define MAX_NR_L2CAP_CHANNELS  (2 * MULTI_SLOT_COUNT) // control + interrupt per slot
#define MAX_NR_L2CAP_SERVICES  3 // GDP + CONTROL + INTERRUPT (shared)
//
#define HCI_ACL_PAYLOAD_SIZE 1021
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 4
#define HCI_OUTGOING_PRE_BUFFER_SIZE 4


#define MAX_NR_RFCOMM_MULTIPLEXERS 0
#define MAX_NR_RFCOMM_SERVICES 0
#define MAX_NR_RFCOMM_CHANNELS 0

// CYW43 específico - necessário para o transport layer

#define NVM_NUM_LINK_KEYS 4
#define NVM_NUM_DEVICE_DB_ENTRIES 4
#define HAVE_EMBEDDED_TIME_MS

// Logging — ENABLE_PRINTF_HEXDUMP must always be defined; hci_dump_embedded_stdout.c
// is compiled unconditionally by the Pico SDK BTstack integration and #errors without it.
#define ENABLE_PRINTF_HEXDUMP

#if !defined(NDEBUG) || (defined(ENABLE_VERBOSE) && ENABLE_VERBOSE)
#define ENABLE_LOG_INFO
#define ENABLE_LOG_ERROR
#endif

#endif
