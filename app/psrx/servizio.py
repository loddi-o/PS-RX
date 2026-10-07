"""
PS-RX - client di alto livello del ricevitore: lo usano app Windows, plugin Decky e CLI.

Ogni metodo fa una o poche richieste di controllo. Se il Pico sparisce dall'USB (succede quando
cambia forma USB: controller collegato o spento, modalita', posti) il client lo ricerca e ripete la
richiesta una volta.
"""

from __future__ import annotations

import struct
import sys
import time
from typing import Callable, Dict, List, Optional

from . import protocollo as p
from .errori import ErrorePsrx, Scollegato, Stallo
from .firmware import Immagine

LUNGHEZZE = {p.CMD_INFO: p.DIM_INFO, p.CMD_STATO: p.DIM_STATO, p.CMD_IMPOSTAZIONI: 64, p.CMD_ABBINATI: 128,
             p.CMD_REGISTRO: 2304, p.CMD_ERRORE: 2, p.CMD_CARICAMENTO: 12, p.CMD_EVENTI: 1 + 16 * p.DIM_EVENTO,
             p.CMD_RETI: p.DIM_RETI, p.CMD_PROVA_RETE: struct.calcsize(p.FMT_PROVA_RETE),
             p.CMD_RETI_VISTE: p.DIM_RETI_VISTE}

T_RICERCA_S = 6.0          # attesa massima del Pico dopo un ricollegamento USB
T_FASE_CARICAMENTO_S = 30  # attesa massima di un blocco o della verifica


class Interrotto(Exception):
    pass


def apri_usb():
    """Trasporto USB della piattaforma (Linux: usbfs, Windows: WinUSB)."""
    if sys.platform == 'win32':
        from . import usb_windows
        return usb_windows.apri()
    from . import usb_linux
    return usb_linux.apri()


class Psrx:
    """apri: funzione senza argomenti che restituisce un trasporto (leggi/scrivi/chiudi) o solleva
    Scollegato. Di default cerca il ricevitore via USB; i test passano il simulatore."""

    def __init__(self, apri: Optional[Callable] = None):
        self._apri = apri or apri_usb
        self._t = None

    # --- trasporto -------------------------------------------------------------------
    def chiudi(self) -> None:
        if self._t is not None:
            try:
                self._t.chiudi()
            finally:
                self._t = None

    def _trasporto(self, attesa_s: float = 0.0):
        fine = time.monotonic() + attesa_s
        while self._t is None:
            try:
                self._t = self._apri()
            except Scollegato:
                if time.monotonic() >= fine:
                    raise
                time.sleep(0.3)
        return self._t

    def _con_riconnessione(self, azione):
        for tentativo in range(2):
            t = self._trasporto(attesa_s=T_RICERCA_S if tentativo else 0.0)
            try:
                return azione(t)
            except Scollegato:
                self.chiudi()
                if tentativo:
                    raise
            except Stallo as e:
                codice, comando = struct.unpack('<BB', t.leggi(p.CMD_ERRORE, lunghezza=2)[:2].ljust(2, b'\0'))
                codice = codice or p.ERR_COMANDO
                raise ErrorePsrx(codice, comando, p.TESTO_ERRORE.get(codice, f'errore {codice}')) from e
        raise Scollegato('PS-RX non trovato')

    def leggi(self, comando: int, indice: int = 0) -> bytes:
        return self._con_riconnessione(lambda t: t.leggi(comando, indice, LUNGHEZZE.get(comando, 2304)))

    def scrivi(self, comando: int, indice: int = 0, dati: bytes = b'', timeout_ms: int = 2000) -> None:
        self._con_riconnessione(lambda t: t.scrivi(comando, indice, dati, timeout_ms))

    # --- letture ---------------------------------------------------------------------------
    def info(self) -> p.Info:
        return p.leggi_info(self.leggi(p.CMD_INFO))

    def stato(self, silenzioso: bool = False) -> p.Stato:
        """silenzioso: lettura di sorveglianza (app in background): il Pico non la conta come app
        aperta, quindi non interroga la radio per l'RSSI."""
        return p.leggi_stato(self.leggi(p.CMD_STATO, p.STATO_SILENZIOSO if silenzioso else 0))

    def impostazioni(self) -> Dict[int, int]:
        return p.leggi_impostazioni(self.leggi(p.CMD_IMPOSTAZIONI))

    def abbinati(self) -> List[p.Abbinato]:
        return p.leggi_abbinati(self.leggi(p.CMD_ABBINATI))

    def registro(self) -> tuple:
        b = self.leggi(p.CMD_REGISTRO)
        n = b[2] | b[3] << 8
        return bool(b[0]), b[4:4 + n].decode('utf-8', 'replace')

    def eventi(self, dopo: int) -> List[p.Evento]:
        return p.leggi_eventi(self.leggi(p.CMD_EVENTI, dopo))

    def reti(self) -> p.Reti:
        return p.leggi_reti(self.leggi(p.CMD_RETI))

    def caricamento(self) -> p.Caricamento:
        return p.leggi_caricamento(self.leggi(p.CMD_CARICAMENTO))

    # --- scritture --------------------------------------------------------------------------
    def imposta(self, ident: int, valore: int) -> None:
        self.scrivi(p.CMD_IMPOSTA, ident, struct.pack('<H', valore))

    def imposta_pad(self, mac: str, ident: int, valore: int) -> None:
        self.scrivi(p.CMD_IMPOSTA_PAD, 0, struct.pack(p.FMT_IMPOSTA_PAD, p.mac_da_testo(mac), ident, valore))

    def salva_ora(self) -> None:
        self.scrivi(p.CMD_SALVA_ORA)

    def predefinite(self) -> None:
        self.scrivi(p.CMD_PREDEFINITE)

    def rinomina(self, mac: str, nome: str) -> None:
        dati = nome.encode('utf-8')[:15]
        self.scrivi(p.CMD_RINOMINA, 0, p.mac_da_testo(mac) + dati + bytes(16 - len(dati)))

    def dimentica(self, mac: str) -> None:
        self.scrivi(p.CMD_DIMENTICA, 0, p.mac_da_testo(mac))

    def dimentica_tutti(self) -> None:
        self.scrivi(p.CMD_DIMENTICA_TUTTI)

    def abbina(self) -> None:
        self.scrivi(p.CMD_ABBINA)

    def spegni_pad(self, posto: Optional[int] = None) -> None:
        self.scrivi(p.CMD_SPEGNI_PAD, p.SLOT_TUTTI if posto is None else posto)

    def salva_rete(self, indice: int, ssid: str, password: str = '', wpa3: bool = False,
                   mantieni_password: bool = False) -> None:
        dati = struct.pack(p.FMT_RETE_DATI, ssid.encode('utf-8')[:32], password.encode('utf-8')[:63],
                           1 if wpa3 else 0, 1 if mantieni_password else 0)
        self.scrivi(p.CMD_RETE_SALVA, indice, dati)

    def cancella_rete(self, indice: int) -> None:
        self.scrivi(p.CMD_RETE_CANCELLA, indice)

    def destinazioni_wol(self, mac1: str = '', mac2: str = '') -> None:
        dati = (p.mac_da_testo(mac1) if mac1 else bytes(6)) + (p.mac_da_testo(mac2) if mac2 else bytes(6))
        self.scrivi(p.CMD_WOL_DESTINAZIONI, 0, dati)

    def cerca_reti(self) -> None:
        """Avvia la ricerca delle reti WiFi vicine (solo senza controller collegati)."""
        self.scrivi(p.CMD_CERCA_RETI)

    def reti_viste(self) -> p.RetiViste:
        return p.leggi_reti_viste(self.leggi(p.CMD_RETI_VISTE))

    def prova_rete(self, indice: int) -> None:
        """Avvia la prova della rete salvata nel posto indice (solo senza controller collegati)."""
        self.scrivi(p.CMD_RETE_PROVA, indice)

    def esito_prova_rete(self) -> p.ProvaRete:
        return p.leggi_prova_rete(self.leggi(p.CMD_PROVA_RETE))

    def prova_wol(self) -> None:
        self.scrivi(p.CMD_WOL_PROVA)

    def annulla_caricamento(self) -> None:
        self.scrivi(p.CMD_CARICA_ANNULLA)

    def installa_ora(self) -> None:
        self.scrivi(p.CMD_INSTALLA_ORA)

    def bootsel_ora(self) -> None:
        self.scrivi(p.CMD_BOOTSEL_ORA)

    # --- aggiornamento del firmware ---------------------------------------------------------------
    def carica_firmware(self, img: Immagine,
                        avanzamento: Callable[[str, int, int], None] = lambda fase, fatto, totale: None,
                        interrotto: Callable[[], bool] = lambda: False, installa: bool = True) -> p.Caricamento:
        """Carica il firmware nell'area di appoggio, aspetta la verifica e (se installa) lo installa:
        il Pico si riavvia col firmware nuovo.

        avanzamento(fase, fatto, totale) con fase in: 'attesa_pad' (serve spegnere i controller),
        'caricamento' (blocchi scritti), 'verifica', 'installazione'."""
        try:
            return self._carica(img, avanzamento, interrotto, installa)
        except Interrotto:
            try:
                self.annulla_caricamento()   # l'area di appoggio torna libera
            except Exception:  # noqa: BLE001 - annullare e' un di piu'
                pass
            raise

    def _carica(self, img: Immagine, avanzamento, interrotto, installa: bool) -> p.Caricamento:
        info = self.info()
        if img.dimensione > info.staging_max:
            raise ErrorePsrx(p.ERR_DIMENSIONE, p.CMD_CARICA_INIZIO, p.TESTO_ERRORE[p.ERR_DIMENSIONE])
        blocchi = img.blocchi()
        self._attendi_senza_pad(avanzamento, interrotto, 0, len(blocchi))
        inizio = struct.pack(p.FMT_CARICA_INIZIO, img.dimensione, img.sha256)
        self._ripeti_se_pad(lambda: self.scrivi(p.CMD_CARICA_INIZIO, 0, inizio), avanzamento, interrotto, 0, len(blocchi))
        for i, blocco in enumerate(blocchi):
            self._attendi_ricezione(i, avanzamento, interrotto, len(blocchi))
            self._ripeti_se_pad(lambda: self.scrivi(p.CMD_CARICA_BLOCCO, i, blocco, timeout_ms=3000),
                                avanzamento, interrotto, i, len(blocchi))
            avanzamento('caricamento', i + 1, len(blocchi))
        self._attendi_ricezione(len(blocchi), avanzamento, interrotto, len(blocchi))
        self.scrivi(p.CMD_CARICA_FINE)
        fine = time.monotonic() + T_FASE_CARICAMENTO_S
        while True:
            c = self.caricamento()
            if c.stato == p.CAR_PRONTO:
                break
            if c.stato == p.CAR_ERRORE:
                raise ErrorePsrx(c.errore, p.CMD_CARICA_FINE, p.TESTO_ERRORE.get(c.errore, '?'))
            if c.stato != p.CAR_VERIFICA or time.monotonic() > fine or interrotto():
                raise ErrorePsrx(p.ERR_SEQUENZA, p.CMD_CARICA_FINE, p.TESTO_ERRORE[p.ERR_SEQUENZA])
            avanzamento('verifica', 0, 1)
            time.sleep(0.1)
        if installa:
            avanzamento('installazione', 0, 1)
            self._ripeti_se_pad(self.installa_ora, avanzamento, interrotto, len(blocchi), len(blocchi))
            self.chiudi()   # il Pico si riavvia: la prossima richiesta lo ricerca
        return c

    def _attendi_senza_pad(self, avanzamento, interrotto, fatto, totale) -> None:
        while self.stato().pad_connessi > 0:
            avanzamento('attesa_pad', fatto, totale)
            if interrotto():
                raise Interrotto()
            time.sleep(1.0)

    def _ripeti_se_pad(self, invio, avanzamento, interrotto, fatto, totale) -> None:
        while True:
            try:
                invio()
                return
            except ErrorePsrx as e:
                if e.codice == p.ERR_PAD_CONNESSO:
                    avanzamento('attesa_pad', fatto, totale)
                    self._attendi_senza_pad(avanzamento, interrotto, fatto, totale)
                elif e.codice == p.ERR_OCCUPATO:
                    time.sleep(0.01)
                else:
                    raise
            if interrotto():
                raise Interrotto()

    def _attendi_ricezione(self, scritti: int, avanzamento, interrotto, totale) -> None:
        """Aspetta che il Pico abbia scritto in flash i blocchi precedenti."""
        fine = time.monotonic() + T_FASE_CARICAMENTO_S
        while True:
            c = self.caricamento()
            if c.stato == p.CAR_RICEZIONE and c.scritti == scritti:
                return
            if c.stato == p.CAR_ERRORE:
                raise ErrorePsrx(c.errore, p.CMD_CARICA_BLOCCO, p.TESTO_ERRORE.get(c.errore, '?'))
            if c.stato not in (p.CAR_RICEZIONE, p.CAR_SCRITTURA):
                raise ErrorePsrx(p.ERR_SEQUENZA, p.CMD_CARICA_BLOCCO, p.TESTO_ERRORE[p.ERR_SEQUENZA])
            if interrotto():
                raise Interrotto()
            if time.monotonic() > fine:
                if self.stato().pad_connessi > 0:
                    avanzamento('attesa_pad', scritti, totale)
                    fine = time.monotonic() + T_FASE_CARICAMENTO_S
                    continue
                raise ErrorePsrx(p.ERR_SEQUENZA, p.CMD_CARICA_BLOCCO, p.TESTO_ERRORE[p.ERR_SEQUENZA])
            time.sleep(0.005)
