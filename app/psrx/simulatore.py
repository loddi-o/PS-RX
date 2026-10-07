"""
PS-RX - ricevitore simulato: risponde al protocollo come il firmware (per test, anteprime e CLI con
--simulatore). Tiene lo stato in memoria; gli errori sono quelli del firmware.
"""

from __future__ import annotations

import hashlib
import struct
import time
from typing import Dict, List

from . import protocollo as p
from .errori import Stallo

CAPACITA = 2_080_768


class PicoSimulato:
    def __init__(self, versione: str = 'ps-rx-0.1.0-simulato'):
        self.versione = versione
        self.richieste = 0
        self.stati_app = 0          # letture di STATO non silenziose (app aperta)
        self.errore = (p.ERR_NESSUNO, 0)
        self.impostazioni: Dict[int, int] = {1: 0, 2: 0, 3: 0, 4: 0, 5: 64, 6: 30, 7: 0, 8: 0, 9: 0, 10: 0, 11: 1}
        mac1, mac2 = p.mac_da_testo('12:34:56:78:9A:BC'), p.mac_da_testo('A0:AB:51:00:11:22')
        self.abbinati: List[dict] = [
            {'mac': mac1, 'nome': '', 'audio': 1, 'microfono': 1, 'polling': 0, 'trackpad': 0, 'inverti': 0},
            {'mac': mac2, 'nome': 'Divano', 'audio': 1, 'microfono': 1, 'polling': 0, 'trackpad': 0, 'inverti': 0},
        ]
        self.pad: List[dict] = [None] * p.PAD_MAX
        self.connetti_pad(0, mac1, p.MODELLO_DUALSENSE, 80)
        self.reti = [{'ssid': '', 'psk': '', 'auth': 0} for _ in range(p.RETI_MAX)]
        self.reti[0] = {'ssid': 'Casa', 'psk': 'password123', 'auth': 0}
        self.wol = [bytes(6), bytes(6)]
        self.rete_stato = p.RETE_SPENTA
        self.prova = None   # (indice, istante d'inizio) della prova del WiFi
        self.scansione = None   # istante d'inizio della ricerca delle reti
        self.wol_inviati = 0
        self.eventi: List[bytes] = []
        self.numero_evento = 0
        self.registro = ['[PS-RX      0.512] PS-RX ps-rx-0.1.0 (simulato)']
        self.car = {'stato': p.CAR_INATTIVO, 'errore': 0, 'scritti': 0, 'totali': 0, 'dim': 0, 'sha': b''}
        self.appoggio = bytearray()
        self.installato = b''
        self.trasporto = _Trasporto(self)
        self.evento(p.EVENTO_COLLEGATO, 0)

    # --- simulazione --------------------------------------------------------------------
    def connetti_pad(self, posto: int, mac: bytes, modello: int = p.MODELLO_DUALSENSE, batteria: int = 80) -> None:
        self.pad[posto] = {'mac': mac, 'modello': modello, 'batteria': batteria, 'carica': False}

    def scollega_pad(self, posto: int) -> None:
        self.pad[posto] = None
        self.evento(p.EVENTO_SCOLLEGATO, posto)

    def evento(self, tipo: int, posto: int, critico: bool = False) -> None:
        self.numero_evento += 1
        x = self.pad[posto] or {'mac': bytes(6), 'modello': 0, 'batteria': 0xFF}
        self.eventi.append(struct.pack(p.FMT_EVENTO, self.numero_evento, tipo, posto, x['modello'], x['batteria'],
                                       1 if critico else 0, self.impostazioni[p.IMP_MODALITA], x['mac']))
        self.eventi = self.eventi[-16:]

    def pad_connessi(self) -> int:
        return sum(1 for x in self.pad if x)

    def _rifiuta(self, codice: int, comando: int):
        self.errore = (codice, comando)
        raise Stallo(comando)

    def _abbinato(self, mac: bytes):
        return next((a for a in self.abbinati if a['mac'] == mac), None)

    # --- letture ------------------------------------------------------------------------
    def leggi(self, comando: int, indice: int) -> bytes:
        self.richieste += 1
        if comando == p.CMD_INFO:
            return struct.pack(p.FMT_INFO, p.MAGIC, p.PROTOCOLLO, p.PAD_MAX, 0, 0x07, CAPACITA,
                               self.versione.encode(), b'DS5-Linux-Bridge 2.3.0-b1')
        if comando == p.CMD_STATO:
            if not indice & p.STATO_SILENZIOSO:
                self.stati_app += 1
            ip = (192, 168, 1, 50) if self.rete_stato == p.RETE_CONNESSA else (0, 0, 0, 0)
            b = struct.pack(p.FMT_STATO, 1, self.impostazioni[p.IMP_MODALITA], self.pad_connessi(), 1, 0,
                            self.pad_connessi(), 2 if False else 0, self.car['stato'], 3600, 60_000,
                            self.numero_evento, self.rete_stato, 0 if self.rete_stato else -1, bytes(ip), -55, 0, 0,
                            0xFFFFFFFF)
            for x in self.pad:
                if x:
                    a = self._abbinato(x['mac']) or {}
                    flag = 1 | (2 if x['carica'] else 0) | (4 if a.get('audio', 1) else 0)
                    b += struct.pack(p.FMT_PAD, 1, x['modello'], x['batteria'], flag, 0, x['mac'], a.get('polling', 0),
                                     250, bytes(6))
                else:
                    b += struct.pack(p.FMT_PAD, 0, 0, 0, 0, 127, bytes(6), 0, 0, bytes(6))
            return b
        if comando == p.CMD_IMPOSTAZIONI:
            return bytes([len(self.impostazioni)]) + b''.join(
                struct.pack(p.FMT_VOCE, i, v) for i, v in sorted(self.impostazioni.items()))
        if comando == p.CMD_ABBINATI:
            b = bytes([len(self.abbinati)])
            for a in self.abbinati:
                posto = next((i for i, x in enumerate(self.pad) if x and x['mac'] == a['mac']), 0xFF)
                nome = a['nome'].encode()[:15]
                b += struct.pack(p.FMT_ABBINATO, a['mac'], nome + bytes(16 - len(nome)), posto, a['audio'],
                                 a['microfono'], a['polling'], a['trackpad'], a['inverti'], 0)
            return b
        if comando == p.CMD_REGISTRO:
            attivo = self.impostazioni[p.IMP_REGISTRO]
            testo = '\n'.join(self.registro[-40:]).encode() if attivo else b''
            return struct.pack('<BBH', attivo, 0, len(testo)) + testo
        if comando == p.CMD_EVENTI:
            nuovi = [e for e in self.eventi if ((struct.unpack_from('<H', e)[0] - indice) & 0xFFFF) not in (0,) and
                     ((struct.unpack_from('<H', e)[0] - indice) & 0xFFFF) < 0x8000]
            return bytes([len(nuovi)]) + b''.join(nuovi)
        if comando == p.CMD_RETI_VISTE:
            if self.scansione is None:
                return bytes([p.SCAN_NESSUNA, 0]) + bytes(p.DIM_RETI_VISTE - 2)
            if time.monotonic() - self.scansione < 1.0:
                return bytes([p.SCAN_IN_CORSO, 0]) + bytes(p.DIM_RETI_VISTE - 2)
            viste = [('Casa', -48, 6, 4), ('Vicino', -71, 11, 4), ('Bar ospiti', -83, 1, 0)]
            b = bytes([p.SCAN_FINITA, len(viste)])
            for ssid, rssi, canale, sic in viste:
                b += struct.pack(p.FMT_RETE_VISTA, ssid.encode(), rssi, canale, sic)
            return b.ljust(p.DIM_RETI_VISTE, b'\0')
        if comando == p.CMD_PROVA_RETE:
            if self.prova is None:
                return struct.pack(p.FMT_PROVA_RETE, 0, 0, -1, 0, bytes(4), bytes(4), 0, 0)
            indice, t0 = self.prova
            passati = time.monotonic() - t0
            if passati < 1.0:
                return struct.pack(p.FMT_PROVA_RETE, p.PROVA_CONNESSIONE, 0, indice, 0, bytes(4), bytes(4), 0,
                                   int(passati * 1000))
            if self.reti[indice]['psk'] == 'sbagliata':
                return struct.pack(p.FMT_PROVA_RETE, p.PROVA_FINITA, p.ESITO_PASSWORD, indice, 0, bytes(4),
                                   bytes(4), 0, 1000)
            return struct.pack(p.FMT_PROVA_RETE, p.PROVA_FINITA, p.ESITO_OK, indice, -55, bytes([192, 168, 1, 50]),
                               bytes([192, 168, 1, 1]), 23, 1200)
        if comando == p.CMD_RETI:
            b = b''
            for r in self.reti:
                b += struct.pack(p.FMT_VOCE_RETE, r['ssid'].encode(), r['auth'], 1 if r['psk'] else 0)
            return b + self.wol[0] + self.wol[1] + bytes([self.impostazioni[p.IMP_WOL_SPENTO]])
        if comando == p.CMD_ERRORE:
            return struct.pack(p.FMT_ERRORE, *self.errore)
        if comando == p.CMD_CARICAMENTO:
            c = self.car
            return struct.pack(p.FMT_CARICAMENTO, c['stato'], c['errore'], c['scritti'], c['totali'], 0, c['dim'])
        self._rifiuta(p.ERR_COMANDO, comando)

    # --- scritture ----------------------------------------------------------------------
    def scrivi(self, comando: int, indice: int, dati: bytes) -> None:
        self.richieste += 1
        if comando == p.CMD_IMPOSTA:
            from .schema import valido
            valore = struct.unpack('<H', dati)[0]
            if not valido(indice, valore):
                self._rifiuta(p.ERR_VALORE, comando)
            self.impostazioni[indice] = valore
            return
        if comando == p.CMD_IMPOSTA_PAD:
            mac, ident, valore = struct.unpack(p.FMT_IMPOSTA_PAD, dati)
            a = self._abbinato(mac)
            if a is None:
                self._rifiuta(p.ERR_NON_TROVATO, comando)
            from .schema import valido_pad
            if not valido_pad(ident, valore):
                self._rifiuta(p.ERR_VALORE, comando)
            chiave = {p.PAD_AUDIO: 'audio', p.PAD_MICROFONO: 'microfono', p.PAD_POLLING: 'polling',
                      p.PAD_TRACKPAD: 'trackpad', p.PAD_INVERTI_SCORRIMENTO: 'inverti'}[ident]
            a[chiave] = valore
            return
        if comando == p.CMD_SALVA_ORA:
            if self.pad_connessi():
                self._rifiuta(p.ERR_PAD_CONNESSO, comando)
            return
        if comando == p.CMD_PREDEFINITE:
            self.impostazioni.update({1: 0, 2: 0, 3: 0, 4: 0, 5: 64, 6: 30, 7: 0, 8: 0, 9: 0, 10: 0})
            return
        if comando == p.CMD_RINOMINA:
            a = self._abbinato(dati[:6])
            if a is None:
                self._rifiuta(p.ERR_NON_TROVATO, comando)
            a['nome'] = dati[6:].split(b'\0', 1)[0].decode('utf-8', 'replace')
            return
        if comando == p.CMD_DIMENTICA:
            a = self._abbinato(dati[:6])
            if a is None:
                self._rifiuta(p.ERR_NON_TROVATO, comando)
            self.abbinati.remove(a)
            for i, x in enumerate(self.pad):
                if x and x['mac'] == a['mac']:
                    self.scollega_pad(i)
            return
        if comando == p.CMD_DIMENTICA_TUTTI:
            self.abbinati.clear()
            for i, x in enumerate(self.pad):
                if x:
                    self.scollega_pad(i)
            return
        if comando == p.CMD_ABBINA:
            self.registro.append('app: abbinamento, finestra di 30 s')
            return
        if comando == p.CMD_SPEGNI_PAD:
            for i, x in enumerate(self.pad):
                if x and (indice == p.SLOT_TUTTI or indice == i):
                    self.scollega_pad(i)
            return
        if comando == p.CMD_RETE_SALVA:
            if indice >= p.RETI_MAX:
                self._rifiuta(p.ERR_VALORE, comando)
            ssid, pw, auth, mantieni = struct.unpack(p.FMT_RETE_DATI, dati)
            ssid, pw = ssid.split(b'\0', 1)[0].decode(), pw.split(b'\0', 1)[0].decode()
            if not ssid or (pw and len(pw) < 8):
                self._rifiuta(p.ERR_VALORE, comando)
            r = self.reti[indice]
            if not (mantieni and r['ssid'] == ssid and not pw):
                r['psk'] = pw
            r['ssid'], r['auth'] = ssid, auth
            return
        if comando == p.CMD_RETE_CANCELLA:
            if indice >= p.RETI_MAX:
                self._rifiuta(p.ERR_VALORE, comando)
            self.reti[indice] = {'ssid': '', 'psk': '', 'auth': 0}
            return
        if comando == p.CMD_WOL_DESTINAZIONI:
            self.wol = [dati[:6], dati[6:12]]
            return
        if comando == p.CMD_CERCA_RETI:
            if self.pad_connessi():
                self._rifiuta(p.ERR_PAD_CONNESSO, comando)
            self.scansione = time.monotonic()
            return
        if comando == p.CMD_RETE_PROVA:
            if indice >= p.RETI_MAX or not self.reti[indice]['ssid']:
                self._rifiuta(p.ERR_NON_TROVATO, comando)
            if self.pad_connessi():
                self._rifiuta(p.ERR_PAD_CONNESSO, comando)
            self.prova = (indice, time.monotonic())
            return
        if comando == p.CMD_WOL_PROVA:
            if self.rete_stato != p.RETE_CONNESSA:
                self._rifiuta(p.ERR_NON_CONNESSO, comando)
            self.wol_inviati += 1
            return
        if comando == p.CMD_BOOTSEL_ORA:
            if self.pad_connessi():
                self._rifiuta(p.ERR_PAD_CONNESSO, comando)
            return
        self._carica(comando, indice, dati)

    def _carica(self, comando: int, indice: int, dati: bytes) -> None:
        c = self.car
        if comando == p.CMD_CARICA_INIZIO:
            if self.pad_connessi():
                self._rifiuta(p.ERR_PAD_CONNESSO, comando)
            dim, sha = struct.unpack(p.FMT_CARICA_INIZIO, dati)
            if dim < 1000 or dim > CAPACITA:
                self._rifiuta(p.ERR_DIMENSIONE, comando)
            totali = (dim + p.SETTORE - 1) // p.SETTORE
            self.car = {'stato': p.CAR_RICEZIONE, 'errore': 0, 'scritti': 0, 'totali': totali, 'dim': dim, 'sha': sha}
            self.appoggio = bytearray(totali * p.SETTORE)
            return
        if comando == p.CMD_CARICA_BLOCCO:
            if self.pad_connessi():
                self._rifiuta(p.ERR_PAD_CONNESSO, comando)
            if c['stato'] != p.CAR_RICEZIONE or indice != c['scritti'] or len(dati) != p.SETTORE:
                self._rifiuta(p.ERR_SEQUENZA, comando)
            self.appoggio[indice * p.SETTORE:(indice + 1) * p.SETTORE] = dati
            c['scritti'] += 1
            return
        if comando == p.CMD_CARICA_FINE:
            if c['stato'] != p.CAR_RICEZIONE or c['scritti'] != c['totali']:
                self._rifiuta(p.ERR_SEQUENZA, comando)
            if hashlib.sha256(bytes(self.appoggio[:c['dim']])).digest() != c['sha']:
                c['stato'], c['errore'] = p.CAR_ERRORE, p.ERR_VERIFICA
            else:
                c['stato'] = p.CAR_PRONTO
            return
        if comando == p.CMD_CARICA_ANNULLA:
            c['stato'] = p.CAR_INATTIVO
            return
        if comando == p.CMD_INSTALLA_ORA:
            if c['stato'] != p.CAR_PRONTO:
                self._rifiuta(p.ERR_NON_PRONTO, comando)
            if self.pad_connessi():
                self._rifiuta(p.ERR_PAD_CONNESSO, comando)
            self.installato = bytes(self.appoggio[:c['dim']])
            c['stato'] = p.CAR_INSTALLAZIONE
            return
        self._rifiuta(p.ERR_COMANDO, comando)


class _Trasporto:
    def __init__(self, pico: PicoSimulato):
        self.pico = pico

    def leggi(self, comando: int, indice: int = 0, lunghezza: int = 2304, timeout_ms: int = 1000) -> bytes:
        return self.pico.leggi(comando, indice)[:lunghezza]

    def scrivi(self, comando: int, indice: int = 0, dati: bytes = b'', timeout_ms: int = 2000) -> None:
        self.pico.scrivi(comando, indice, dati)

    def chiudi(self) -> None:
        pass
