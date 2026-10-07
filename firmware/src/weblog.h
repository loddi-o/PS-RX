//
// RAM capture of stdio (all printf diagnostics) so the firmware log can be
// read from a browser at /api/log -- no UART adapter needed. Two sections:
// the first KB of output is frozen forever (boot diagnostics survive hours
// of HCI chatter), and a rolling ring keeps the most recent KB. RAM-ONLY:
// log bytes never touch flash. Gated by the persisted web toggle
// Config_body.weblog_enabled (default OFF); the persistence is what lets a
// user enable it, reproduce an issue across a reboot (boot logs captured),
// and copy-paste the log from the browser.
//

#ifndef DS5_BRIDGE_WEBLOG_H
#define DS5_BRIDGE_WEBLOG_H

#ifdef ENABLE_WIFI_WOL

// Prepare the capture driver (registered DISABLED). Call once, right after
// board_init(), so weblog_set_enabled(true) can start capturing as early as
// config_load() has run.
void weblog_init();

// Attach/detach the capture driver from pico stdio. Live-applied from the
// web UI toggle and at boot from the persisted config. Cheap and idempotent.
void weblog_set_enabled(bool enabled);
bool weblog_enabled();

// Copy the captured log (boot section + gap marker + recent tail, oldest
// first) into out, NUL-terminated. Returns bytes written (excluding the NUL).
int weblog_snapshot(char *out, int cap);

#else
// The log is only readable through the web UI; without the WiFi transport
// there is no reader, so don't spend the RAM.
static inline void weblog_init() {}
static inline void weblog_set_enabled(bool) {}
static inline bool weblog_enabled() { return false; }
static inline int weblog_snapshot(char *, int) { return 0; }
#endif // ENABLE_WIFI_WOL

#endif // DS5_BRIDGE_WEBLOG_H
