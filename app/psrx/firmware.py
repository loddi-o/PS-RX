"""
PS-RX - lettura e controllo di un firmware da caricare con l'app (.bin oppure .uf2).

Gli stessi controlli li rifa' il Pico dopo la scrittura (SHA-256 + immagine RP2350): qui servono
a fermarsi subito, prima di caricare un file sbagliato.
"""

from __future__ import annotations

import hashlib
import os
import re
import struct
from dataclasses import dataclass
from typing import List, Optional

from .protocollo import SETTORE

FLASH = 0x10000000
SRAM, SRAM_FINE = 0x20000000, 0x20082000
INIZIO_BLOCCO, FINE_BLOCCO = 0xFFFFDED3, 0xAB123579
MINIMO = 64 * 1024

# UF2
UF2_MAGIC0, UF2_MAGIC1, UF2_MAGIC_FINE = 0x0A324655, 0x9E5D5157, 0x0AB16F30
UF2_FLAG_NON_FLASH = 0x00000001
UF2_FLAG_FAMIGLIA = 0x00002000
FAMIGLIA_RP2350_ARM_S = 0xE48BFF59


class FirmwareNonValido(Exception):
    pass


@dataclass
class Immagine:
    nome: str
    dati: bytes
    sha256: bytes
    versione: Optional[str]

    @property
    def dimensione(self) -> int:
        return len(self.dati)

    def blocchi(self) -> List[bytes]:
        """Settori da 4 KB; l'ultimo completato con 0xFF (come la flash cancellata)."""
        risultato = []
        for i in range(0, len(self.dati), SETTORE):
            pezzo = self.dati[i:i + SETTORE]
            risultato.append(pezzo + b'\xff' * (SETTORE - len(pezzo)))
        return risultato


def da_uf2(contenuto: bytes) -> bytes:
    """Immagine binaria dai blocchi UF2 della famiglia RP2350 ARM (sicura) nella flash."""
    if len(contenuto) % 512:
        raise FirmwareNonValido('file UF2 di lunghezza non multipla di 512')
    pezzi = {}
    for i in range(0, len(contenuto), 512):
        blocco = contenuto[i:i + 512]
        m0, m1, flag, indirizzo, dim, _n, _tot, famiglia = struct.unpack_from('<IIIIIIII', blocco)
        if m0 != UF2_MAGIC0 or m1 != UF2_MAGIC1 or struct.unpack_from('<I', blocco, 508)[0] != UF2_MAGIC_FINE:
            raise FirmwareNonValido('blocco UF2 non valido')
        if flag & UF2_FLAG_NON_FLASH:
            continue
        if not (flag & UF2_FLAG_FAMIGLIA) or famiglia != FAMIGLIA_RP2350_ARM_S:
            continue        # per esempio il blocco "absolute" dell'erratum E10 di picotool
        if dim > 476 or not (FLASH <= indirizzo < FLASH + 16 * 1024 * 1024):
            continue
        pezzi[indirizzo] = blocco[32:32 + dim]
    if not pezzi:
        raise FirmwareNonValido('nessun blocco RP2350 nel file UF2')
    if min(pezzi) != FLASH:
        raise FirmwareNonValido('il firmware non parte dall\'inizio della flash')
    fine = max(a + len(d) for a, d in pezzi.items())
    immagine = bytearray(b'\xff' * (fine - FLASH))
    for indirizzo, dati in pezzi.items():
        immagine[indirizzo - FLASH:indirizzo - FLASH + len(dati)] = dati
    return bytes(immagine)


def problemi(dati: bytes, capacita: Optional[int] = None) -> List[str]:
    """Elenco dei motivi per cui l'immagine non va caricata (vuoto = va bene)."""
    errori = []
    if len(dati) < MINIMO:
        errori.append(f'troppo piccolo ({len(dati)} byte)')
    if capacita is not None and len(dati) > capacita:
        errori.append(f'troppo grande: {len(dati)} byte, al massimo {capacita}')
    if len(dati) < 4096:
        return errori or ['file troppo corto']
    sp, reset = struct.unpack_from('<II', dati)
    if not (SRAM <= sp <= SRAM_FINE) or sp & 7:
        errori.append('tabella dei vettori non valida (stack)')
    if not (reset & 1) or not (FLASH <= (reset & ~1) < FLASH + len(dati)):
        errori.append('tabella dei vettori non valida (reset)')
    parole = struct.unpack_from('<1024I', dati)
    try:
        i = parole.index(INIZIO_BLOCCO, 2)
        if FINE_BLOCCO not in parole[i + 1:]:
            errori.append('blocco IMAGE_DEF incompleto')
    except ValueError:
        errori.append('manca il blocco IMAGE_DEF: non è un firmware RP2350')
    return errori


def versione(dati: bytes) -> Optional[str]:
    """Versione PS-RX scritta nell'immagine (binary info), se c'e'."""
    m = re.search(rb'ps-rx-[0-9A-Za-z.\-_]+', dati)
    return m.group(0).decode() if m else None


def leggi(percorso: str, capacita: Optional[int] = None) -> Immagine:
    with open(percorso, 'rb') as f:
        contenuto = f.read()
    if percorso.lower().endswith('.uf2') or contenuto[:4] == struct.pack('<I', UF2_MAGIC0):
        dati = da_uf2(contenuto)
    else:
        dati = contenuto
    errori = problemi(dati, capacita)
    if errori:
        raise FirmwareNonValido('; '.join(errori))
    return Immagine(os.path.basename(percorso), dati, hashlib.sha256(dati).digest(), versione(dati))
