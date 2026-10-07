//
// Test della logica pura di PS-RX con tempo simulato. Si compila sul PC con -DPSRX_TEST_HOST
// (strumenti/prova_logica.ps1). Ogni gruppo stampa il suo nome; alla fine il totale.
//

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "caricamento.h"
#include "coda_uart.h"
#include "combo.h"
#include "conta_click.h"
#include "ds4.h"
#include "eventi.h"
#include "politica_rete.h"
#include "politiche.h"
#include "psrx_config.h"

namespace {

int fallimenti = 0;
int controlli = 0;

void verifica(bool ok, const char *descrizione, int riga) {
    controlli++;
    if (!ok) {
        fallimenti++;
        printf("  FALLITO (riga %d): %s\n", riga, descrizione);
    }
}

#define VERIFICA(cond) verifica((cond), #cond, __LINE__)

// --- Coda del log verso la UART ------------------------------------------------

template <uint32_t N>
std::string svuota(CodaUart<N> &c, size_t massimo = 1u << 20) {
    std::string s;
    char ch;
    while (s.size() < massimo && c.leggi(ch)) s += ch;
    return s;
}

void test_coda_uart() {
    printf("[test] coda del log verso la UART\n");
    {
        CodaUart<64> c;
        std::string atteso;
        for (int i = 0; i < 40; i++) {
            const std::string riga = "riga " + std::to_string(i) + "\r\n";
            c.scrivi(riga.c_str(), static_cast<int>(riga.size()));
            atteso += riga;
            if (i % 3 == 2) {
                VERIFICA(svuota(c) == atteso);
                atteso.clear();
            }
        }
        VERIFICA(svuota(c) == atteso);
    }
    {
        CodaUart<64> c;
        const std::string a(60, 'a');
        c.scrivi(a.c_str(), 60);
        c.scrivi("0123456789", 10);
        VERIFICA(c.persi() == 10);
        VERIFICA(svuota(c) == a);
        c.scrivi("ok\r\n", 4);
        VERIFICA(svuota(c) == "\r\n[... 10 caratteri di log persi]\r\nok\r\n");
    }
}

// --- Politica del WiFi e del Wake-on-LAN ----------------------------------------

struct SimRete {
    PoliticaRete p;
    uint32_t ora = 100000;
    int pad = 0;
    bool link = false;
    bool wol = true;
    bool reti = true;
    int pacchetti = 0;
    bool acceso = false;

    // Avanza di 'ms' a passi di 10 ms (come il ciclo del firmware).
    void avanza(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 10) {
            const DecisioneRete d = p.aggiorna(ora, pad, link && acceso, wol, reti);
            if (d.invia_wol) pacchetti++;
            acceso = d.wifi_acceso;
            ora += 10;
        }
    }
};

void test_politica_rete() {
    printf("[test] WiFi e Wake-on-LAN\n");
    {   // avvio senza controller: WiFi subito acceso
        SimRete s;
        s.avanza(10);
        VERIFICA(s.acceso);
    }
    {   // nessuna rete salvata: WiFi spento
        SimRete s;
        s.reti = false;
        s.avanza(1000);
        VERIFICA(!s.acceso);
    }
    {   // primo pad con WiFi gia' connesso: 3 pacchetti a 300 ms, poi WiFi spento
        SimRete s;
        s.link = true;
        s.avanza(1000);
        s.pad = 1;
        s.avanza(10);
        VERIFICA(s.pacchetti == 1);
        VERIFICA(s.acceso);
        s.avanza(290);
        VERIFICA(s.pacchetti == 1);
        s.avanza(20);
        VERIFICA(s.pacchetti == 2);
        s.avanza(400);
        VERIFICA(s.pacchetti == 3);
        s.avanza(20);
        VERIFICA(!s.acceso);
        s.avanza(60000);
        VERIFICA(s.pacchetti == 3);
        VERIFICA(!s.acceso);
    }
    {   // WiFi non ancora connesso: aspetta, poi manda appena connesso
        SimRete s;
        s.avanza(100);
        s.pad = 1;
        s.avanza(5000);
        VERIFICA(s.pacchetti == 0);
        VERIFICA(s.acceso);
        s.link = true;
        s.avanza(1000);
        VERIFICA(s.pacchetti == 3);
        s.avanza(20);
        VERIFICA(!s.acceso);
    }
    {   // il WiFi non si connette mai: dopo 30 s si spegne senza pacchetti
        SimRete s;
        s.avanza(100);
        s.pad = 1;
        s.avanza(29900);
        VERIFICA(s.acceso);
        s.avanza(200);
        VERIFICA(!s.acceso);
        VERIFICA(s.pacchetti == 0);
        s.link = true;
        s.avanza(5000);
        VERIFICA(s.pacchetti == 0);
    }
    {   // WoL non configurato: WiFi spento subito al primo pad
        SimRete s;
        s.wol = false;
        s.link = true;
        s.avanza(1000);
        s.pad = 1;
        s.avanza(10);
        VERIFICA(!s.acceso);
        VERIFICA(s.pacchetti == 0);
    }
    {   // secondo pad: nessun nuovo WoL; scollegati tutti, il WiFi torna dopo 2 s
        SimRete s;
        s.link = true;
        s.avanza(1000);
        s.pad = 1;
        s.avanza(2000);
        VERIFICA(s.pacchetti == 3);
        s.pad = 2;
        s.avanza(2000);
        VERIFICA(s.pacchetti == 3);
        VERIFICA(!s.acceso);
        s.pad = 0;
        s.avanza(1900);
        VERIFICA(!s.acceso);
        s.avanza(200);
        VERIFICA(s.acceso);
        s.pad = 1;      // un nuovo primo pad: altro giro di WoL
        s.avanza(1000);
        VERIFICA(s.pacchetti == 6);
    }
    {   // pad collegato e scollegato prima che il WiFi si connetta: niente WoL
        SimRete s;
        s.avanza(100);
        s.pad = 1;
        s.avanza(1000);
        s.pad = 0;
        s.avanza(100);
        s.link = true;
        s.avanza(5000);
        VERIFICA(s.pacchetti == 0);
        VERIFICA(s.acceso);
    }
}

// --- Click del BOOTSEL e caricamento del firmware (come PS250) -------------------

struct SimClick {
    ContaClick c;
    uint32_t ora = 1000;
    std::vector<uint8_t> serie;
    void tieni(bool premuto, uint32_t ms) {
        for (uint32_t x = 0; x < ms; x += T_BOOTSEL_LETTURA_MS) {
            const uint8_t n = c.aggiorna(premuto, ora);
            if (n) serie.push_back(n);
            ora += T_BOOTSEL_LETTURA_MS;
        }
    }
};

void test_conta_click() {
    printf("[test] click del BOOTSEL\n");
    {   // un click: la serie si chiude dopo 600 ms
        SimClick s;
        s.tieni(true, 200);
        s.tieni(false, 500);
        VERIFICA(s.serie.empty());
        s.tieni(false, 300);
        VERIFICA(s.serie.size() == 1 && s.serie[0] == 1);
    }
    {   // tre click ravvicinati
        SimClick s;
        for (int i = 0; i < 3; i++) { s.tieni(true, 200); s.tieni(false, 300); }
        s.tieni(false, 1000);
        VERIFICA(s.serie.size() == 1 && s.serie[0] == 3);
    }
    {   // due click lontani: due serie da uno
        SimClick s;
        s.tieni(true, 200); s.tieni(false, 1500);
        s.tieni(true, 200); s.tieni(false, 1500);
        VERIFICA(s.serie.size() == 2 && s.serie[0] == 1 && s.serie[1] == 1);
    }
    {   // pressione lunga: nessun click, anche se seguita da altri click nella stessa serie
        SimClick s;
        s.tieni(true, 2000); s.tieni(false, 300);
        s.tieni(true, 200); s.tieni(false, 1000);
        VERIFICA(s.serie.empty());
        s.tieni(true, 200); s.tieni(false, 1000);       // la serie dopo vale di nuovo
        VERIFICA(s.serie.size() == 1 && s.serie[0] == 1);
    }
    {   // azzera a meta' serie
        SimClick s;
        s.tieni(true, 200); s.tieni(false, 200);
        s.c.azzera();
        s.tieni(false, 1000);
        VERIFICA(s.serie.empty());
    }
}

void test_caricamento() {
    printf("[test] caricamento del firmware\n");
    const uint8_t sha[32] = {1, 2, 3};
    {   // percorso completo: 3 settori (l'ultimo parziale), verifica, pronto, installazione
        Caricamento c(2u * 1024 * 1024, 1000);
        VERIFICA(c.stato() == CAR_INATTIVO && !c.in_corso());
        VERIFICA(c.inizia(2 * PSRX_SETTORE + 100, sha) == ERR_NESSUNO);
        VERIFICA(c.stato() == CAR_RICEZIONE && c.settori_totali() == 3 && c.in_corso());
        VERIFICA(c.sha256()[2] == 3 && c.dimensione() == 2 * PSRX_SETTORE + 100);
        for (uint16_t i = 0; i < 3; i++) {
            VERIFICA(c.controlla_blocco(i, PSRX_SETTORE) == ERR_NESSUNO && c.stato() == CAR_RICEZIONE);
            VERIFICA(c.controlla_blocco(i + 1, PSRX_SETTORE) == ERR_SEQUENZA);
            VERIFICA(c.accetta_blocco(i, PSRX_SETTORE) == ERR_NESSUNO);
            VERIFICA(c.controlla_blocco(i + 1, PSRX_SETTORE) == ERR_OCCUPATO);
            VERIFICA(c.stato() == CAR_SCRITTURA && c.blocco_in_scrittura() == i);
            VERIFICA(c.accetta_blocco(i + 1, PSRX_SETTORE) == ERR_OCCUPATO);   // il precedente non e' scritto
            VERIFICA(c.fine() == ERR_OCCUPATO);
            c.blocco_scritto(true);
            VERIFICA(c.settori_scritti() == i + 1 && c.stato() == CAR_RICEZIONE);
        }
        VERIFICA(c.accetta_blocco(3, PSRX_SETTORE) == ERR_SEQUENZA);           // oltre la fine
        VERIFICA(c.fine() == ERR_NESSUNO && c.stato() == CAR_VERIFICA);
        VERIFICA(c.inizia(5000, sha) == ERR_OCCUPATO);                         // non durante la verifica
        c.verificato(ERR_NESSUNO);
        VERIFICA(c.stato() == CAR_PRONTO && !c.in_corso());
        c.installazione();
        VERIFICA(c.stato() == CAR_INSTALLAZIONE);
    }
    {   // dimensioni fuori dai limiti: rifiutate, stato invariato
        Caricamento c(10000, 2000);
        VERIFICA(c.inizia(1999, sha) == ERR_DIMENSIONE && c.stato() == CAR_INATTIVO);
        VERIFICA(c.inizia(10001, sha) == ERR_DIMENSIONE && c.stato() == CAR_INATTIVO);
        VERIFICA(c.inizia(10000, sha) == ERR_NESSUNO && c.settori_totali() == 3);
    }
    {   // blocchi fuori ordine o corti, FINE anticipata, blocco senza INIZIO
        Caricamento c(100000, 100);
        VERIFICA(c.accetta_blocco(0, PSRX_SETTORE) == ERR_SEQUENZA);
        VERIFICA(c.inizia(3 * PSRX_SETTORE, sha) == ERR_NESSUNO);
        VERIFICA(c.accetta_blocco(1, PSRX_SETTORE) == ERR_SEQUENZA);
        VERIFICA(c.accetta_blocco(0, 100) == ERR_LUNGHEZZA);
        VERIFICA(c.fine() == ERR_SEQUENZA);
        VERIFICA(c.stato() == CAR_RICEZIONE);
    }
    {   // scrittura fallita, verifica fallita, annulla
        Caricamento c(100000, 100);
        c.inizia(PSRX_SETTORE, sha);
        c.accetta_blocco(0, PSRX_SETTORE);
        c.blocco_scritto(false);
        VERIFICA(c.stato() == CAR_ERRORE && c.codice_errore() == ERR_FLASH);
        VERIFICA(c.inizia(PSRX_SETTORE, sha) == ERR_NESSUNO && c.codice_errore() == ERR_NESSUNO);  // si riparte
        c.accetta_blocco(0, PSRX_SETTORE);
        c.blocco_scritto(true);
        c.fine();
        c.verificato(ERR_VERIFICA);
        VERIFICA(c.stato() == CAR_ERRORE && c.codice_errore() == ERR_VERIFICA);
        c.annulla();
        VERIFICA(c.stato() == CAR_INATTIVO && c.settori_totali() == 0);
        c.installazione();                                                      // non pronto: niente
        VERIFICA(c.stato() == CAR_INATTIVO);
    }
    {   // immagine RP2350: vettori e blocco IMAGE_DEF
        std::vector<uint8_t> img(8192, 0xFF);
        auto metti = [&](uint32_t pos, uint32_t v) {
            img[pos] = v & 0xFF; img[pos + 1] = (v >> 8) & 0xFF; img[pos + 2] = (v >> 16) & 0xFF; img[pos + 3] = v >> 24;
        };
        metti(0, 0x20082000u); metti(4, 0x1000015du); metti(312, 0xffffded3u); metti(328, 0xab123579u);
        VERIFICA(immagine_rp2350_valida(img.data(), 8192));
        VERIFICA(!immagine_rp2350_valida(img.data(), 4000));                   // troppo corta
        metti(4, 0x10000158u);                                                  // reset senza bit Thumb
        VERIFICA(!immagine_rp2350_valida(img.data(), 8192));
        metti(4, 0x10003001u);                                                  // reset fuori dall'immagine
        VERIFICA(!immagine_rp2350_valida(img.data(), 8192));
        metti(4, 0x1000015du); metti(0, 0x30000000u);                           // stack fuori dalla SRAM
        VERIFICA(!immagine_rp2350_valida(img.data(), 8192));
        metti(0, 0x20082000u); metti(312, 0);                                   // senza IMAGE_DEF
        VERIFICA(!immagine_rp2350_valida(img.data(), 8192));
        metti(312, 0xffffded3u); metti(328, 0);                                 // blocco senza fine
        VERIFICA(!immagine_rp2350_valida(img.data(), 8192));
    }
}


// --- Politiche su flash e salvataggio ---------------------------------------------

void test_politiche() {
    printf("[test] flash e salvataggio differito\n");
    VERIFICA(flash_libera(0, false));
    VERIFICA(!flash_libera(1, false));
    VERIFICA(!flash_libera(0, true));
    const uint32_t MAI = 0xFFFFFFFFu;
    VERIFICA(!deve_salvare(false, false, 0, false, 10000, MAI));       // niente da salvare
    VERIFICA(!deve_salvare(true, false, 0, false, 1000, MAI));         // modifica troppo recente
    VERIFICA(deve_salvare(true, false, 0, false, 2000, MAI));
    VERIFICA(!deve_salvare(true, false, 1, false, 60000, MAI));        // controller collegato
    VERIFICA(!deve_salvare(true, true, 0, true, 60000, MAI));          // collegamento in corso
    VERIFICA(deve_salvare(true, true, 0, false, 0, MAI));              // salva ora
    VERIFICA(!deve_salvare(true, false, 0, false, 60000, 5000));       // fallito da poco
    VERIFICA(deve_salvare(true, false, 0, false, 60000, 10000));
}

// --- Eventi per le notifiche -----------------------------------------------------------

struct SimEventi {
    Eventi e;
    PadPerEventi pad[PSRX_PAD_MAX] = {};
    uint32_t ora = 1000;
    uint16_t letto = 0;
    std::vector<EventoPsrx> nuovi;

    void avanza(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 20) {
            e.aggiorna(ora, pad, 0);
            ora += 20;
        }
        EventoPsrx buf[Eventi::N_EVENTI];
        const uint8_t n = e.leggi(letto, buf, Eventi::N_EVENTI);
        nuovi.assign(buf, buf + n);
        if (n) letto = buf[n - 1].numero;
    }
    void collega(uint8_t posto, uint8_t modello, bool valida, uint8_t batteria) {
        PadPerEventi &p = pad[posto];
        p.connesso = true;
        p.modello = modello;
        p.batteria_valida = valida;
        p.batteria = batteria;
        p.in_carica = false;
        for (int i = 0; i < 6; i++) p.mac[i] = static_cast<uint8_t>(0x10 * posto + i);
    }
};

void test_eventi() {
    printf("[test] eventi per le notifiche\n");
    {   // collegato con batteria subito nota, poi scollegato
        SimEventi s;
        s.collega(1, MODELLO_DS4, true, 70);
        s.avanza(40);
        VERIFICA(s.nuovi.size() == 1);
        VERIFICA(s.nuovi[0].tipo == EVENTO_COLLEGATO && s.nuovi[0].posto == 1);
        VERIFICA(s.nuovi[0].modello == MODELLO_DS4 && s.nuovi[0].batteria == 70);
        VERIFICA(s.nuovi[0].mac[0] == 0x10);
        s.pad[1].connesso = false;
        s.avanza(40);
        VERIFICA(s.nuovi.size() == 1 && s.nuovi[0].tipo == EVENTO_SCOLLEGATO && s.nuovi[0].posto == 1);
    }
    {   // batteria non ancora nota: si aspetta fino a 3 s
        SimEventi s;
        s.collega(0, MODELLO_DUALSENSE, false, 0);
        s.avanza(1000);
        VERIFICA(s.nuovi.empty());
        s.pad[0].batteria_valida = true;
        s.pad[0].batteria = 50;
        s.avanza(40);
        VERIFICA(s.nuovi.size() == 1 && s.nuovi[0].batteria == 50);
        SimEventi t;
        t.collega(0, MODELLO_DUALSENSE, false, 0);
        t.avanza(3100);
        VERIFICA(t.nuovi.size() == 1 && t.nuovi[0].batteria == 0xFF);   // batteria sconosciuta
    }
    {   // soglie 20% e 10%, una volta sola; riarmo sopra il 40%
        SimEventi s;
        s.collega(0, MODELLO_DUALSENSE, true, 50);
        s.avanza(40);
        s.pad[0].batteria = 30;
        s.avanza(100);
        VERIFICA(s.nuovi.empty());
        s.pad[0].batteria = 20;
        s.avanza(100);
        VERIFICA(s.nuovi.size() == 1 && s.nuovi[0].tipo == EVENTO_BATTERIA && s.nuovi[0].flag == 0);
        s.avanza(100);
        VERIFICA(s.nuovi.empty());
        s.pad[0].batteria = 10;
        s.avanza(100);
        VERIFICA(s.nuovi.size() == 1 && s.nuovi[0].flag == 1);
        s.pad[0].batteria = 0;
        s.avanza(100);
        VERIFICA(s.nuovi.empty());
        s.pad[0].in_carica = true;
        s.pad[0].batteria = 20;
        s.avanza(100);
        VERIFICA(s.nuovi.empty());
        s.pad[0].in_carica = false;
        s.avanza(100);
        VERIFICA(s.nuovi.size() == 1 && s.nuovi[0].flag == 0);   // riarmato dalla carica
    }
    {   // collegato gia' scarico: la notifica di collegamento basta, niente avviso doppio
        SimEventi s;
        s.collega(2, MODELLO_DUALSENSE, true, 10);
        s.avanza(200);
        VERIFICA(s.nuovi.size() == 1 && s.nuovi[0].tipo == EVENTO_COLLEGATO);
    }
    {   // l'app legge dopo molti eventi: tiene gli ultimi N_EVENTI, in ordine
        SimEventi s;
        for (int i = 0; i < 12; i++) {
            s.collega(0, MODELLO_DUALSENSE, true, 80);
            s.e.aggiorna(s.ora += 20, s.pad, 0);
            s.pad[0].connesso = false;
            s.e.aggiorna(s.ora += 20, s.pad, 0);
        }
        EventoPsrx buf[Eventi::N_EVENTI];
        const uint8_t n = s.e.leggi(0, buf, Eventi::N_EVENTI);
        VERIFICA(n == Eventi::N_EVENTI);
        VERIFICA(buf[n - 1].numero == 24 && buf[0].numero == 9);
        VERIFICA(s.e.leggi(24, buf, Eventi::N_EVENTI) == 0);
        VERIFICA(s.e.leggi(22, buf, Eventi::N_EVENTI) == 2);
    }
}

// --- DualShock 4 presentato come DualSense ------------------------------------------

// Report 0x11 come arriva dall'L2CAP: A1 11 C0 00 + report USB del DualShock 4 da [1].
struct ReportDs4 {
    uint8_t b[79] = {};
    ReportDs4() {
        b[0] = 0xA1;
        b[1] = 0x11;
        b[2] = 0xC0;
        for (int k = 1; k <= 4; k++) usb(k) = 0x80;   // sticks al centro
        usb(5) = 0x08;                                  // croce direzionale rilasciata
        usb(35) = 0x80;                                 // nessun tocco
        usb(39) = 0x80;
    }
    uint8_t &usb(int k) { return b[k + 3]; }
};

void test_ds4() {
    printf("[test] DualShock 4 come DualSense\n");
    uint8_t neutro[DS5_LUNGHEZZA_REPORT];
    memset(neutro, 0, sizeof neutro);
    neutro[0] = neutro[1] = neutro[2] = neutro[3] = 0x80;
    neutro[7] = 0x08;
    {   // tasti, sticks, grilletti, batteria
        ReportDs4 r;
        r.usb(1) = 0x10; r.usb(2) = 0x20; r.usb(3) = 0x30; r.usb(4) = 0x40;
        r.usb(5) = 0x20 | 0x02;          // croce + destra
        r.usb(6) = 0x01 | 0x10 | 0x20;   // L1, Share, Options
        r.usb(7) = 0x01 | 0x02 | (5 << 2); // PS, click del touchpad, contatore 5
        r.usb(8) = 200; r.usb(9) = 100;
        r.usb(13) = 0x34; r.usb(14) = 0x12;   // primo valore del giroscopio
        r.usb(30) = 0x07;                // batteria 70%, senza cavo
        StatoDs4 st;
        uint8_t out[DS5_LUNGHEZZA_REPORT];
        VERIFICA(ds4_in_dualsense(r.b, sizeof r.b, neutro, st, out));
        VERIFICA(out[0] == 0x10 && out[1] == 0x20 && out[2] == 0x30 && out[3] == 0x40);
        VERIFICA(out[4] == 200 && out[5] == 100);
        VERIFICA(out[6] == 5);
        VERIFICA(out[7] == (0x20 | 0x02));
        VERIFICA(out[8] == (0x01 | 0x10 | 0x20));
        VERIFICA(out[9] == 0x03);
        VERIFICA(out[15] == 0x34 && out[16] == 0x12);
        VERIFICA(out[52] == 0x07);       // livello 7, scarica
        r.usb(30) = 0x10 | 0x05;         // cavo, 50%
        ds4_in_dualsense(r.b, sizeof r.b, neutro, st, out);
        VERIFICA(out[52] == (0x05 | 0x10));
        r.usb(30) = 0x10 | 11;           // cavo, carica completa
        ds4_in_dualsense(r.b, sizeof r.b, neutro, st, out);
        VERIFICA(out[52] == (10 | 0x20));
    }
    {   // touchpad: stessa forma, y riscalata da 942 a 1079
        ReportDs4 r;
        r.usb(35) = 0x05;                // tocco attivo, id 5
        const uint16_t x = 1000, y = 942;
        r.usb(36) = x & 0xFF;
        r.usb(37) = static_cast<uint8_t>((x >> 8) | (y & 0x0F) << 4);
        r.usb(38) = static_cast<uint8_t>(y >> 4);
        StatoDs4 st;
        uint8_t out[DS5_LUNGHEZZA_REPORT];
        ds4_in_dualsense(r.b, sizeof r.b, neutro, st, out);
        const uint16_t x5 = static_cast<uint16_t>(out[33] | (out[34] & 0x0F) << 8);
        const uint16_t y5 = static_cast<uint16_t>((out[34] >> 4) | out[35] << 4);
        VERIFICA(out[32] == 0x05);
        VERIFICA(x5 == 1000 && y5 == 1079);
        VERIFICA(out[36] == 0x80);       // secondo punto: nessun tocco
    }
    {   // timestamp continuo anche quando il contatore a 16 bit riparte
        ReportDs4 r;
        StatoDs4 st;
        uint8_t out[DS5_LUNGHEZZA_REPORT];
        uint32_t ts;
        r.usb(10) = 0xF0; r.usb(11) = 0xFF;    // 65520
        ds4_in_dualsense(r.b, sizeof r.b, neutro, st, out);
        memcpy(&ts, &out[27], 4);
        VERIFICA(ts == 0);
        r.usb(10) = 0x10; r.usb(11) = 0x00;    // 16: +32 unita' da 5,33 us
        ds4_in_dualsense(r.b, sizeof r.b, neutro, st, out);
        memcpy(&ts, &out[27], 4);
        VERIFICA(ts == 32 * 16);
    }
    {   // pacchetti non validi
        ReportDs4 r;
        StatoDs4 st;
        uint8_t out[DS5_LUNGHEZZA_REPORT];
        VERIFICA(!ds4_in_dualsense(r.b, 20, neutro, st, out));
        r.b[1] = 0x31;
        VERIFICA(!ds4_in_dualsense(r.b, sizeof r.b, neutro, st, out));
    }
    {   // uscita: rumble e lightbar dallo stato del DualSense
        uint8_t stato[63] = {};
        stato[2] = 40;     // motore leggero
        stato[3] = 200;    // motore pesante
        stato[44] = 1; stato[45] = 2; stato[46] = 3;
        uint8_t out[DS4_LUNGHEZZA_USCITA];
        ds4_uscita(stato, 4, out);
        VERIFICA(out[0] == 0x11 && out[1] == 0xC4 && out[3] == 0x07);
        VERIFICA(out[6] == 40 && out[7] == 200);
        VERIFICA(out[8] == 1 && out[9] == 2 && out[10] == 3);
    }
    {   // calibrazione riordinata e report sintetici
        uint8_t cal[41];
        for (int i = 0; i < 41; i++) cal[i] = static_cast<uint8_t>(i);
        ds4_calibrazione_in_dualsense(cal, sizeof cal);
        // DS4: [7]=pitch+ [9]=yaw+ [11]=roll+ [13]=pitch- [15]=yaw- [17]=roll-
        VERIFICA(cal[7] == 7 && cal[9] == 13 && cal[11] == 9 && cal[13] == 15 && cal[15] == 11 && cal[17] == 17);
        VERIFICA(cal[1] == 1 && cal[19] == 19);
        uint8_t f09[DS5_LUNGHEZZA_FEATURE_09];
        const uint8_t mac[6] = {0xA0, 0xAB, 0x51, 0x01, 0x02, 0x03};
        ds4_feature_09(mac, f09);
        VERIFICA(f09[0] == 0x09 && f09[1] == 0x03 && f09[6] == 0xA0);
        uint8_t f20[DS5_LUNGHEZZA_FEATURE_20];
        ds4_feature_20(f20);
        VERIFICA(f20[0] == 0x20 && f20[44] == 0x24 && f20[45] == 0x02);
    }
}

// --- Combinazione di spegnimento ------------------------------------------------------

void test_combo() {
    printf("[test] combinazione Share/Create + Options + L2 + R2\n");
    ComboSpegni c;
    VERIFICA(!c.aggiorna(0x04 | 0x08 | 0x10));            // manca Options
    VERIFICA(c.aggiorna(COMBO_SPEGNI));                     // completa: scatta
    VERIFICA(!c.aggiorna(COMBO_SPEGNI));                    // tenuta: non riscatta
    VERIFICA(!c.aggiorna(0x10));                            // rilascio parziale: non riarma
    VERIFICA(!c.aggiorna(COMBO_SPEGNI));
    VERIFICA(!c.aggiorna(0x01 | 0x02));                     // L1 R1: tutto il resto rilasciato
    VERIFICA(c.aggiorna(COMBO_SPEGNI | 0x01));              // altri tasti non contano
}

} // namespace

int esegui_test_logica() {
    fallimenti = 0;
    controlli = 0;
    test_coda_uart();
    test_politica_rete();
    test_politiche();
    test_eventi();
    test_conta_click();
    test_caricamento();
    test_ds4();
    test_combo();
    printf("[test] %d controlli, %d falliti: %s\n", controlli, fallimenti, fallimenti ? "ERRORE" : "OK");
    return fallimenti;
}

#ifdef PSRX_TEST_HOST
int main() {
    return esegui_test_logica() ? 1 : 0;
}
#endif
