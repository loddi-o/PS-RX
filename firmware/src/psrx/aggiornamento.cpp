//
// PS-RX - aggiornamento del firmware dall'app (vedi aggiornamento.h).
//
// Disposizione della flash (4 MB), la stessa dell'OTA di DS5-Linux-Bridge:
//   [0, 2 MB)            firmware in esecuzione
//   [2 MB, -4 settori)   area di appoggio del firmware caricato
//   -4 settori           configurazione (config.cpp)
//   -3, -2 settori       abbinamenti BTstack
//   -1 settore           cancellato dal bootrom a ogni caricamento UF2
//

#include "aggiornamento.h"

#include <cstring>

#include "caricamento.h"
#include "log_psrx.h"
#include "pad.h"
#include "politiche.h"
#include "uart_asincrona.h"

#include "hardware/flash.h"
#include "hardware/structs/psm.h"
#include "hardware/structs/watchdog.h"
#include "hardware/sync.h"
#include "pico/bootrom.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/sha256.h"
#include "pico/time.h"

static constexpr uint32_t APPOGGIO = 2u * 1024u * 1024u;
static constexpr uint32_t LIMITE = PICO_FLASH_SIZE_BYTES - 4u * FLASH_SECTOR_SIZE; // = configurazione
static constexpr uint32_t CAPACITA = LIMITE - APPOGGIO;
static constexpr uint32_t MINIMO = 64u * 1024u;
static constexpr uint32_t T_SENZA_BLOCCHI_MS = 60000; // caricamento abbandonato dall'app
static constexpr uint32_t PEZZO_VERIFICA = 64u * 1024u;
static_assert(APPOGGIO % FLASH_SECTOR_SIZE == 0, "appoggio non allineato");
static_assert(PSRX_SETTORE == FLASH_SECTOR_SIZE, "blocco diverso dal settore di flash");
static_assert(CAPACITA >= 1024u * 1024u, "flash troppo piccola per l'appoggio");

static Caricamento car(CAPACITA, MINIMO);
// Riceve i blocchi dall'USB e, all'installazione, fa da appoggio in RAM per la copia.
static uint8_t settore[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));
static uint32_t t_ultimo_blocco = 0;
static bool installa_subito = false;
static bool bootsel_subito = false;

static pico_sha256_state_t sha;
static bool sha_attivo = false;
static uint32_t sha_pos = 0;

static uint32_t ora_ms() {
    return to_ms_since_boot(get_absolute_time());
}

// --- scrittura in flash -------------------------------------------------------------

struct Scrittura {
    uint32_t offset;
    const uint8_t *dati;
};

static void scrivi_op(void *param) {
    const auto *w = static_cast<const Scrittura *>(param);
    flash_range_erase(w->offset, FLASH_SECTOR_SIZE);
    flash_range_program(w->offset, w->dati, FLASH_SECTOR_SIZE);
}

// Come config_save(): core 1 parcheggiato e interrupt spenti durante la scrittura, con qualche
// tentativo se il core 1 non si ferma in tempo. Poi rilettura di controllo.
static bool scrivi_settore(uint32_t offset, const uint8_t *dati) {
    Scrittura w{offset, dati};
    int rc = PICO_ERROR_TIMEOUT;
    for (int tentativo = 0; tentativo < 3 && rc != PICO_OK; tentativo++) {
        rc = flash_safe_execute(scrivi_op, &w, 1000);
    }
    if (rc != PICO_OK) return false;
    return memcmp(reinterpret_cast<const void *>(XIP_BASE + offset), dati, FLASH_SECTOR_SIZE) == 0;
}

static void scrivi_blocco() {
    const uint16_t i = car.blocco_in_scrittura();
    const bool ok = scrivi_settore(APPOGGIO + static_cast<uint32_t>(i) * FLASH_SECTOR_SIZE, settore);
    car.blocco_scritto(ok);
    if (!ok) psrx_log("aggiornamento: scrittura del blocco %u FALLITA", i);
}

// --- verifica ------------------------------------------------------------------------

static void ferma_verifica() {
    if (sha_attivo) {
        pico_sha256_cleanup(&sha);
        sha_attivo = false;
    }
}

// SHA-256 hardware dell'RP2350, a pezzi da 64 KB per non tenere fermo il ciclo principale.
static void passo_verifica() {
    const uint8_t *base = reinterpret_cast<const uint8_t *>(XIP_BASE + APPOGGIO);
    const uint32_t dim = car.dimensione();
    if (!sha_attivo) {
        if (pico_sha256_try_start(&sha, SHA256_BIG_ENDIAN, false) != PICO_OK) return; // riprovo al giro dopo
        sha_attivo = true;
        sha_pos = 0;
    }
    const uint32_t n = dim - sha_pos < PEZZO_VERIFICA ? dim - sha_pos : PEZZO_VERIFICA;
    pico_sha256_update_blocking(&sha, base + sha_pos, n);
    sha_pos += n;
    if (sha_pos < dim) return;

    sha256_result_t calcolato;
    pico_sha256_finish(&sha, &calcolato);
    sha_attivo = false;
    uint8_t esito = ERR_NESSUNO;
    if (memcmp(calcolato.bytes, car.sha256(), sizeof calcolato.bytes) != 0) esito = ERR_VERIFICA;
    else if (!immagine_rp2350_valida(base, dim)) esito = ERR_IMMAGINE;
    car.verificato(esito);
    if (esito == ERR_NESSUNO) {
        psrx_log("aggiornamento: firmware nuovo verificato (%lu byte), pronto da installare",
                 static_cast<unsigned long>(dim));
    } else {
        psrx_log("aggiornamento: verifica FALLITA (%s)",
                 esito == ERR_VERIFICA ? "SHA-256 diverso" : "non e' un firmware RP2350");
    }
}

// --- installazione -----------------------------------------------------------------

// Tutto quello che tocca deve stare in RAM: copia l'appoggio sopra il firmware in esecuzione.
// flash_range_erase/program sono gia' in RAM nell'SDK; la copia e' un ciclo con letture volatile,
// cosi' il compilatore non lo trasforma in una chiamata a memcpy (in flash). Il riavvio finale e'
// un TRIGGER diretto del watchdog. Interrupt spenti dal chiamante.
static void __attribute__((noreturn)) __no_inline_not_in_flash_func(copia_e_riavvia)(uint32_t *ram, uint32_t dimensione) {
    for (uint32_t off = 0; off < dimensione; off += FLASH_SECTOR_SIZE) {
        const volatile uint32_t *da = reinterpret_cast<const volatile uint32_t *>(XIP_BASE + APPOGGIO + off);
        for (uint32_t k = 0; k < FLASH_SECTOR_SIZE / 4; k++) ram[k] = da[k];
        flash_range_erase(off, FLASH_SECTOR_SIZE);
        flash_range_program(off, reinterpret_cast<const uint8_t *>(ram), FLASH_SECTOR_SIZE);
    }
    watchdog_hw->ctrl = WATCHDOG_CTRL_TRIGGER_BITS;
    while (true) {
        tight_loop_contents();
    }
}

static void installa() {
    const uint32_t dim = car.dimensione();
    const uint32_t copia = (dim + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    car.installazione();
    psrx_log("aggiornamento: INSTALLO il firmware nuovo (%lu byte). Non staccare il ricevitore per circa 15 s",
             static_cast<unsigned long>(dim));
    uart_asincrona_svuota_tutto();

    // Da qui non si torna indietro: core 1 fermo (non deve leggere la flash), watchdog spento (la
    // copia dura piu' del suo periodo), riavvio completo del chip al TRIGGER (scratch[4] = 0: il
    // bootrom fa l'avvio normale dalla flash, cioe' nel firmware nuovo).
    multicore_reset_core1();
    hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
    watchdog_hw->scratch[4] = 0;
    hw_set_bits(&psm_hw->wdsel, PSM_WDSEL_BITS & ~(PSM_WDSEL_ROSC_BITS | PSM_WDSEL_XOSC_BITS));
    save_and_disable_interrupts();
    copia_e_riavvia(reinterpret_cast<uint32_t *>(settore), copia);
}

void aggiornamento_entra_in_bootsel(const char *motivo) {
    psrx_log("entro nella modalita' aggiornamento (BOOTSEL): %s", motivo);
    uart_asincrona_svuota_tutto();
    reset_usb_boot(0, 0);
}

// --- ciclo principale ------------------------------------------------------------------

void aggiornamento_task() {
    const int pad = pad_connessi();
    const bool libera = flash_libera(pad, pad_setup_bt());

    if (car.stato() == CAR_SCRITTURA && libera) {
        scrivi_blocco();
        t_ultimo_blocco = ora_ms();
    }
    if (car.stato() == CAR_VERIFICA && libera) passo_verifica();
    if (car.stato() == CAR_RICEZIONE && ora_ms() - t_ultimo_blocco > T_SENZA_BLOCCHI_MS) {
        psrx_log("aggiornamento: caricamento interrotto (nessun blocco da %lu s)",
                 static_cast<unsigned long>(T_SENZA_BLOCCHI_MS / 1000));
        car.errore(ERR_SEQUENZA);
    }
    if (bootsel_subito) {
        bootsel_subito = false;
        if (pad == 0) aggiornamento_entra_in_bootsel("richiesta dall'app");
    }
    if (installa_subito) {
        installa_subito = false;
        if (car.stato() == CAR_PRONTO && libera) installa();
    }
}

// --- servizio USB ----------------------------------------------------------------------

uint8_t aggiornamento_inizia(uint32_t dimensione, const uint8_t sha256[32]) {
    if (pad_connessi() > 0) return ERR_PAD_CONNESSO;
    ferma_verifica();
    const uint8_t esito = car.inizia(dimensione, sha256);
    if (esito == ERR_NESSUNO) {
        t_ultimo_blocco = ora_ms();
        psrx_log("aggiornamento: ricevo un firmware nuovo di %lu byte (%u blocchi)",
                 static_cast<unsigned long>(dimensione), car.settori_totali());
    }
    return esito;
}

uint8_t aggiornamento_prepara_blocco(uint16_t indice, uint16_t lunghezza, uint8_t **destinazione) {
    if (pad_connessi() > 0) return ERR_PAD_CONNESSO;
    const uint8_t esito = car.controlla_blocco(indice, lunghezza);
    if (esito == ERR_NESSUNO) *destinazione = settore;
    return esito;
}

uint8_t aggiornamento_blocco_ricevuto(uint16_t indice, uint16_t lunghezza) {
    t_ultimo_blocco = ora_ms();
    return car.accetta_blocco(indice, lunghezza);
}

uint8_t aggiornamento_fine() {
    const uint8_t esito = car.fine();
    if (esito == ERR_NESSUNO) psrx_log("aggiornamento: tutti i blocchi scritti, verifico");
    return esito;
}

void aggiornamento_annulla() {
    ferma_verifica();
    if (car.stato() != CAR_INATTIVO) psrx_log("aggiornamento: annullato");
    car.annulla();
}

uint8_t aggiornamento_installa_ora() {
    if (car.stato() != CAR_PRONTO) return ERR_NON_PRONTO;
    if (pad_connessi() > 0) return ERR_PAD_CONNESSO;
    installa_subito = true;
    return ERR_NESSUNO;
}

uint8_t aggiornamento_bootsel_ora() {
    if (pad_connessi() > 0) return ERR_PAD_CONNESSO;
    bootsel_subito = true;
    return ERR_NESSUNO;
}

uint8_t aggiornamento_stato() {
    return car.stato();
}

void aggiornamento_leggi(CaricamentoPsrx *out) {
    out->stato = car.stato();
    out->errore = car.stato() == CAR_ERRORE ? car.codice_errore() : ERR_NESSUNO;
    out->settori_scritti = car.settori_scritti();
    out->settori_totali = car.settori_totali();
    out->riservato = 0;
    out->dimensione = car.dimensione();
}

uint32_t aggiornamento_capacita() {
    return CAPACITA;
}
