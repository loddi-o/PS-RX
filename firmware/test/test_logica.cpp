//
// Test della logica pura di PS-RX con tempo simulato. Si compila sul PC con -DPSRX_TEST_HOST
// (strumenti/prova_logica.ps1). Ogni gruppo stampa il suo nome; alla fine il totale.
//

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "coda_uart.h"
#include "politica_rete.h"

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

} // namespace

int esegui_test_logica() {
    fallimenti = 0;
    controlli = 0;
    test_coda_uart();
    test_politica_rete();
    printf("[test] %d controlli, %d falliti: %s\n", controlli, fallimenti, fallimenti ? "ERRORE" : "OK");
    return fallimenti;
}

#ifdef PSRX_TEST_HOST
int main() {
    return esegui_test_logica() ? 1 : 0;
}
#endif
