"""
PS-RX - trasporto USB su Linux: richieste di controllo vendor con usbfs, solo libreria standard.

Si cercano in /sys/bus/usb/devices i dispositivi con una delle identita' del ricevitore (DualSense,
Xbox 360, Steam Controller: dipende dalla modalita'), si apre /dev/bus/usb/BBB/DDD e si manda
CMD_INFO: un controller vero risponde STALL o con dati diversi, PS-RX con il magic "PSRX".

usbfs ammette le richieste vendor anche con i driver del kernel agganciati (hid-playstation, xpad,
snd-usb-audio), ma serve il permesso di scrittura sul nodo: regola udev (permessi.py) o root.
"""

from __future__ import annotations

import array
import errno
import glob
import os
import struct
from dataclasses import dataclass
from typing import List

from . import protocollo as p
from .errori import PermessoNegato, Scollegato, Stallo

try:
    import fcntl
except ImportError:   # non Linux
    fcntl = None

# struct usbdevfs_ctrltransfer { u8 bRequestType, bRequest; u16 wValue, wIndex, wLength; u32 timeout; void *data; }
if struct.calcsize('P') == 8:
    _FMT_CTRL = '=BBHHHI4xQ'
else:
    _FMT_CTRL = '=BBHHHII'
_DIM_CTRL = struct.calcsize(_FMT_CTRL)
USBDEVFS_CONTROL = (3 << 30) | (_DIM_CTRL << 16) | (ord('U') << 8) | 0   # _IOWR('U', 0, ...)

_LETTURA = 0xC0   # vendor, dispositivo, IN
_SCRITTURA = 0x40  # vendor, dispositivo, OUT


@dataclass
class Candidato:
    percorso: str      # /dev/bus/usb/BBB/DDD
    vid: int
    pid: int
    prodotto: str


def _leggi(percorso: str) -> str:
    try:
        with open(percorso) as f:
            return f.read().strip()
    except OSError:
        return ''


def trova_candidati() -> List[Candidato]:
    trovati = []
    for cartella in sorted(glob.glob('/sys/bus/usb/devices/*')):
        vid, pid = _leggi(os.path.join(cartella, 'idVendor')), _leggi(os.path.join(cartella, 'idProduct'))
        if not vid or not pid or (int(vid, 16), int(pid, 16)) not in p.IDENTITA:
            continue
        bus, dev = _leggi(os.path.join(cartella, 'busnum')), _leggi(os.path.join(cartella, 'devnum'))
        if not bus or not dev:
            continue
        trovati.append(Candidato(f'/dev/bus/usb/{int(bus):03d}/{int(dev):03d}', int(vid, 16), int(pid, 16),
                                 _leggi(os.path.join(cartella, 'product'))))
    return trovati


class TrasportoUsbfs:
    """Richieste di controllo vendor su un nodo usbfs aperto."""

    def __init__(self, percorso: str):
        self.percorso = percorso
        try:
            self._fd = os.open(percorso, os.O_RDWR)
        except PermissionError as e:
            raise PermessoNegato(percorso) from e
        except FileNotFoundError as e:
            raise Scollegato(percorso) from e

    def chiudi(self) -> None:
        if self._fd is not None:
            os.close(self._fd)
            self._fd = None

    def _controllo(self, tipo: int, comando: int, indice: int, buffer: array.array, lunghezza: int,
                   timeout_ms: int) -> int:
        indirizzo = buffer.buffer_info()[0] if lunghezza else 0
        ctrl = bytearray(struct.pack(_FMT_CTRL, tipo, p.RICHIESTA, comando, indice, lunghezza, timeout_ms, indirizzo))
        try:
            return fcntl.ioctl(self._fd, USBDEVFS_CONTROL, ctrl, True)
        except OSError as e:
            if e.errno == errno.EPIPE:
                raise Stallo(comando) from e
            if e.errno in (errno.ENODEV, errno.ENOENT, errno.ESHUTDOWN, errno.EPROTO, errno.EIO):
                raise Scollegato(self.percorso) from e
            raise

    def leggi(self, comando: int, indice: int = 0, lunghezza: int = 2304, timeout_ms: int = 1000) -> bytes:
        buffer = array.array('B', bytes(lunghezza))
        n = self._controllo(_LETTURA, comando, indice, buffer, lunghezza, timeout_ms)
        return buffer.tobytes()[:n]

    def scrivi(self, comando: int, indice: int = 0, dati: bytes = b'', timeout_ms: int = 2000) -> None:
        buffer = array.array('B', dati)
        self._controllo(_SCRITTURA, comando, indice, buffer, len(dati), timeout_ms)


def apri() -> TrasportoUsbfs:
    """Il primo dispositivo che risponde come PS-RX. PermessoNegato se ce n'e' uno candidato ma non
    si puo' aprire (e nessun altro risponde); Scollegato se non ce n'e' nessuno."""
    negato = None
    for c in trova_candidati():
        try:
            t = TrasportoUsbfs(c.percorso)
        except PermessoNegato as e:
            negato = e
            continue
        except Scollegato:
            continue
        try:
            if t.leggi(p.CMD_INFO, lunghezza=p.DIM_INFO)[:4] == p.MAGIC:
                return t
        except (Stallo, Scollegato, OSError):
            pass
        t.chiudi()
    if negato:
        raise negato
    raise Scollegato('nessun PS-RX collegato')
