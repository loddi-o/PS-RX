"""
PS-RX - installazione del firmware su un Pico 2 W nuovo (mai programmato) o in modalita' BOOTSEL.

Un Pico 2 W con la memoria vuota si avvia da solo nel bootloader dell'RP2350, che si presenta come una
chiavetta USB ("RP2350", con il file INFO_UF2.TXT). Lo stesso succede a un Pico qualsiasi collegato tenendo
premuto BOOTSEL, o a un PS-RX con il comando "Riavvia in BOOTSEL". Copiando un file .uf2 nella chiavetta il
Pico scrive il firmware e si riavvia: niente driver, niente strumenti esterni.

- Windows: si cercano le unita' rimovibili con INFO_UF2.TXT che nomina l'RP2350.
- Linux: le chiavette gia' montate (desktop: le monta udisks); come root (plugin Decky) si monta da se'
  il disco del bootloader (vendor "RPI", modello "RP2350") in una cartella temporanea.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import time
from typing import Callable, List, Optional
from .lingua import tr

INFO = 'INFO_UF2.TXT'
NOME_FILE = 'ps-rx.uf2'


def _e_rp2350(cartella: str) -> bool:
    try:
        with open(os.path.join(cartella, INFO), encoding='ascii', errors='replace') as f:
            testo = f.read(512)
    except OSError:
        return False
    return 'RP2350' in testo


def _unita_windows() -> List[str]:
    import ctypes
    k32 = ctypes.windll.kernel32
    maschera = k32.GetLogicalDrives()
    trovate = []
    vecchia = k32.SetErrorMode(1)   # niente finestre "inserire un disco" su lettori vuoti
    try:
        for i in range(26):
            if not maschera & (1 << i):
                continue
            radice = f'{chr(65 + i)}:\\'
            if k32.GetDriveTypeW(ctypes.c_wchar_p(radice)) != 2:   # DRIVE_REMOVABLE
                continue
            if _e_rp2350(radice):
                trovate.append(radice)
    finally:
        k32.SetErrorMode(vecchia)
    return trovate


def _montate_linux() -> List[str]:
    trovate = []
    try:
        with open('/proc/mounts') as f:
            righe = f.read().splitlines()
    except OSError:
        return trovate
    for riga in righe:
        campi = riga.split()
        if len(campi) >= 3 and campi[2] in ('vfat', 'msdos', 'fuseblk'):
            punto = campi[1].replace('\\040', ' ')
            if _e_rp2350(punto):
                trovate.append(punto)
    return trovate


def dischi_bootloader_linux() -> List[str]:
    """Dischi a blocchi del bootloader RP2350 (/dev/sdX), montati o no."""
    trovati = []
    base = '/sys/block'
    try:
        voci = os.listdir(base)
    except OSError:
        return trovati
    for nome in voci:
        try:
            with open(os.path.join(base, nome, 'device', 'vendor')) as f:
                vendor = f.read().strip()
            with open(os.path.join(base, nome, 'device', 'model')) as f:
                modello = f.read().strip()
        except OSError:
            continue
        if vendor == 'RPI' and modello.startswith('RP2350'):
            trovati.append('/dev/' + nome)
    return trovati


def trova() -> List[str]:
    """Cartelle (lettere d'unita' su Windows) dei Pico in modalita' BOOTSEL, pronte a ricevere un .uf2."""
    if sys.platform == 'win32':
        return _unita_windows()
    return _montate_linux()


def presente() -> bool:
    """C'e' un Pico in BOOTSEL (anche non montato, su Linux)."""
    return bool(trova()) or (sys.platform != 'win32' and bool(dischi_bootloader_linux()))


def scrivi_uf2(cartella: str, uf2: str, avanzamento: Callable[[int, int], None] = lambda f, t: None) -> None:
    """Copia il firmware nella chiavetta del bootloader. Alla fine il Pico si riavvia da solo e la chiavetta
    sparisce: gli errori dopo che tutti i byte sono stati scritti non contano."""
    totale = os.path.getsize(uf2)
    fatti = 0
    destinazione = os.path.join(cartella, NOME_FILE)
    with open(uf2, 'rb') as sorgente:
        out = open(destinazione, 'wb')
        try:
            while True:
                pezzo = sorgente.read(64 * 1024)
                if not pezzo:
                    break
                out.write(pezzo)
                fatti += len(pezzo)
                avanzamento(fatti, totale)
            out.flush()
            os.fsync(out.fileno())
        except OSError:
            if fatti < totale:
                raise
        finally:
            try:
                out.close()
            except OSError:
                if fatti < totale:
                    raise


def scrivi_uf2_come_root(uf2: str, avanzamento: Callable[[int, int], None] = lambda f, t: None) -> None:
    """Linux come root (plugin Decky): usa una chiavetta gia' montata, oppure monta da se' il disco del
    bootloader in una cartella temporanea."""
    montate = _montate_linux()
    if montate:
        scrivi_uf2(montate[0], uf2, avanzamento)
        return
    dischi = dischi_bootloader_linux()
    if not dischi:
        raise FileNotFoundError(tr("nessun Pico in modalita' BOOTSEL"))
    disco = dischi[0]
    partizione = disco + '1' if os.path.exists(disco + '1') else disco
    cartella = tempfile.mkdtemp(prefix='ps-rx-pico-')
    subprocess.run(['mount', '-t', 'vfat', partizione, cartella], check=True, capture_output=True, timeout=20)
    try:
        if not _e_rp2350(cartella):
            raise FileNotFoundError(tr("il disco montato non e' il bootloader dell'RP2350"))
        scrivi_uf2(cartella, uf2, avanzamento)
        subprocess.run(['sync'], timeout=20)
    finally:
        subprocess.run(['umount', '-l', cartella], capture_output=True, timeout=20)
        try:
            os.rmdir(cartella)
        except OSError:
            pass


def attendi_psrx(apri: Callable, timeout_s: float = 40.0, interrotto: Callable[[], bool] = lambda: False):
    """Aspetta che il Pico appena programmato si presenti come PS-RX. Restituisce il trasporto aperto."""
    from .errori import Scollegato
    fine = time.monotonic() + timeout_s
    while time.monotonic() < fine and not interrotto():
        try:
            return apri()
        except Scollegato:
            time.sleep(0.5)
    raise TimeoutError(tr("il Pico non si e' presentato come PS-RX"))


def firmware_incluso() -> Optional[str]:
    """Firmware messo nell'exe al momento della build (PyInstaller), per installare anche senza internet."""
    base = getattr(sys, '_MEIPASS', None)
    if not base:
        return None
    percorso = os.path.join(base, 'firmware', 'ps-rx-firmware.uf2')
    return percorso if os.path.exists(percorso) else None
