//
// mbedtls_config.h -- minimal mbedTLS 3.6 config for the GitHub OTA client.
// (The filename is the SDK contract: pico_mbedtls_headers defines
// MBEDTLS_CONFIG_FILE="pico_mbedtls_config.h", whose wrapper does exactly
// #include "mbedtls_config.h" -- resolved to this file via the target's
// include path. Only ENABLE_OTA builds link pico_mbedtls, so this file is
// inert in every other build.)
//
// Scope: TLS 1.2 CLIENT ONLY, exactly what the two GitHub endpoints need
// (github.com -> Sectigo/USERTrust ECC chain, ECDHE-ECDSA-AES-GCM;
// objects.githubusercontent.com -> Let's Encrypt/ISRG RSA chain,
// ECDHE-RSA-AES-GCM). TLS 1.3 is deliberately OFF: it would drag in PSA
// crypto (+~50 KB flash, more heap) for zero benefit -- GitHub negotiates
// 1.2 fine.
//
// RAM: the TLS I/O buffers are the big cost -- IN must stay 16384 (a peer may
// send full-size records; we can't shrink it without the Max Fragment Length
// extension, which CDNs routinely ignore), OUT is 2048 (we only send a tiny
// GET + handshake flights). ~18.5 KB of heap held for the life of a
// connection, plus handshake temporaries. That is exactly why OTA runs in a
// dedicated boot mode with BT/audio never initialised: the Opus codec states
// that normally eat the heap were never allocated, leaving ~90 KB free.
//
// No RTC on the Pico, so MBEDTLS_HAVE_TIME is NOT defined: X.509 validity
// periods are not checked (mbedTLS skips them without a time source). The
// trust anchor is the pinned root set in ota_certs.h; expiry checking would
// require NTP for marginal benefit on a firmware-download path that is
// additionally SHA-256-verified against the release manifest.
//

#ifndef OTA_MBEDTLS_CONFIG_H
#define OTA_MBEDTLS_CONFIG_H

// Workaround for some mbedtls source files using INT_MAX without including
// limits.h (same workaround as the SDK's own test config).
#include <limits.h>

// --- Platform -------------------------------------------------------------
#define MBEDTLS_PLATFORM_C
// The SDK's patched altcp_tls_mbedtls.c shim reaches into mbedTLS 3.x
// private struct fields (ssl.out_left, session.start) -- these three defines
// are the SDK's own convention for building against it (test/kitchen_sink).
// MBEDTLS_HAVE_TIME gives session structs their time fields; note it is NOT
// MBEDTLS_HAVE_TIME_DATE, so X.509 validity periods stay unchecked (no RTC --
// see the header note). mbedtls_ms_time comes from pico_mbedtls.
#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_PLATFORM_MS_TIME_ALT
// Entropy comes from the SDK's hardware poll (pico_mbedtls.c -> get_rand_64);
// there is no /dev/urandom here.
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C
// Thumb asm for bignum inner loops: meaningful RSA/ECC handshake speedup.
#define MBEDTLS_HAVE_ASM

// --- TLS 1.2 client -------------------------------------------------------
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
// GitHub serves by SNI; without it the CDN presents a default cert.
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
// IN must fit a full 16 KB TLS record (no MFL negotiation -- see header note).
#define MBEDTLS_SSL_IN_CONTENT_LEN  16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 2048

// --- Ciphers / hashes -----------------------------------------------------
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CIPHER_C
// AES tables in flash, and the smaller set: OTA mode has no audio to protect
// from XIP pressure, and the ~8 KB of RAM matters more than AES throughput.
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_AES_FEWER_TABLES
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C  // TLS 1.2 PRF, cert sigs, and the firmware image hash
#define MBEDTLS_SHA384_C  // Sectigo E46 chain signs with ECDSA-SHA384
#define MBEDTLS_SHA512_C  // prerequisite of SHA384 in mbedTLS 3.x
// PARSE-only need (HW-observed 2026-07-11): DigiCert Global Root CA (2006) is
// self-signed with SHA-1; without SHA1_C its signature OID doesn't map and
// mbedtls_x509_crt_parse counts it as a failed cert -- and the SDK's
// altcp_tls_create_config_client treats ANY failed cert in the bundle as
// fatal (returns NULL -> "TLS init failed"). Root SELF-signatures are never
// cryptographically verified, so this buys OID recognition, not SHA-1 trust.
#define MBEDTLS_SHA1_C

// --- Key exchange / signature crypto ---------------------------------------
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED  // github.com leaf (ECDSA P-256)
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED  // USERTrust/Sectigo roots (P-384)
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED // preferred ECDHE group
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_RSA_C     // ISRG/Let's Encrypt chain (RSA-2048/4096 certs)
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21 // RSA-PSS cert signatures (some LE intermediates)
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C

// --- X.509 / PEM ------------------------------------------------------------
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
// The pinned root bundle (ota_certs.h) is concatenated PEM.
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_BASE64_C

// --- Diagnostics ------------------------------------------------------------
// Readable mbedtls_strerror() on the UART for failed handshakes; a few KB of
// flash, invaluable when a CA rotation eventually breaks the pinned bundle.
#define MBEDTLS_ERROR_C
// mbedTLS debug callback (handshake state/errors -> UART via the altcp shim's
// registered hook; see ALTCP_MBEDTLS_LIB_DEBUG in lwipopts.h). The verbosity
// threshold is raised from its silent default in ota.cpp. Only fires in OTA
// boot mode -- there is no TLS traffic anywhere else.
#define MBEDTLS_DEBUG_C

#endif // OTA_MBEDTLS_CONFIG_H
