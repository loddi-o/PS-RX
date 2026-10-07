//
// PS-RX - coda circolare dei caratteri del log verso la UART (logica pura, testata sul PC).
//
// Un solo produttore alla volta (l'out_chars dello stdio, sotto il print_mutex dell'SDK, da
// qualsiasi core) e un solo consumatore (il ciclo principale del core 0). Se un pezzo non ci sta
// si scarta intero e si contano i caratteri persi; appena c'e' di nuovo posto, prima del pezzo
// successivo, va in coda l'avviso "[... N caratteri di log persi]".
//

#ifndef PSRX_CODA_UART_H
#define PSRX_CODA_UART_H

#include <atomic>
#include <cstdint>
#include <cstdio>

template <uint32_t N>
class CodaUart {
    static_assert(N >= 64 && (N & (N - 1)) == 0, "N deve essere una potenza di 2");

public:
    // Produttore. Mette 'len' caratteri in coda, oppure nessuno se non c'e' posto.
    void scrivi(const char *s, int len) {
        if (len <= 0) return;
        if (persi_ > 0) {
            char avviso[48];
            const int n = snprintf(avviso, sizeof avviso, "\r\n[... %lu caratteri di log persi]\r\n",
                                   static_cast<unsigned long>(persi_));
            if (n > 0 && static_cast<uint32_t>(n) <= libero()) {
                copia(avviso, static_cast<uint32_t>(n));
                persi_ = 0;
            }
        }
        if (persi_ > 0 || static_cast<uint32_t>(len) > libero()) {
            persi_ += static_cast<uint32_t>(len);
            return;
        }
        copia(s, static_cast<uint32_t>(len));
    }

    // Consumatore. Toglie un carattere; false se la coda e' vuota.
    bool leggi(char &c) {
        const uint32_t l = letti_.load(std::memory_order_relaxed);
        if (l == scritti_.load(std::memory_order_acquire)) return false;
        c = buf_[l & (N - 1)];
        letti_.store(l + 1, std::memory_order_release);
        return true;
    }

    uint32_t livello() const {
        return scritti_.load(std::memory_order_acquire) - letti_.load(std::memory_order_acquire);
    }
    uint32_t persi() const { return persi_; }

private:
    uint32_t libero() const { return N - livello(); }

    void copia(const char *s, uint32_t n) {
        const uint32_t w = scritti_.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < n; i++) buf_[(w + i) & (N - 1)] = s[i];
        scritti_.store(w + n, std::memory_order_release);
    }

    char buf_[N] = {};
    std::atomic<uint32_t> scritti_{0}; // totale dei caratteri messi in coda
    std::atomic<uint32_t> letti_{0};   // totale dei caratteri tolti
    uint32_t persi_ = 0;               // scartati e non ancora segnalati (solo il produttore)
};

#endif // PSRX_CODA_UART_H
