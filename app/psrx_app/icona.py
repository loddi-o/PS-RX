"""
PS-RX app - icone dai file in risorse/ (set "1a-carosello", originali in grafica/icone/).

- Finestra ed exe: icona-16/32/48 (ridisegnate per le misure piccole), icona-256 e icona-1024.
- Area di notifica: vassoio-attivo (ricevitore collegato) e vassoio-spento (non collegato), a 16 e 32 px.
"""

from __future__ import annotations

import os
import struct

from PySide6.QtCore import QBuffer, QByteArray, QIODevice, Qt
from PySide6.QtGui import QIcon, QPixmap

RISORSE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'risorse')
MISURE_APP = (16, 32, 48, 256, 1024)
# Misure dentro il file .ico: quelle mancanti fra i file si ricavano dalla misura disegnata piu' vicina.
MISURE_ICO = (16, 20, 24, 32, 40, 48, 64, 96, 256)


def _file(nome: str) -> QPixmap:
    return QPixmap(os.path.join(RISORSE, nome))


def icona() -> QIcon:
    """Icona della finestra (e della barra delle applicazioni)."""
    ic = QIcon()
    for lato in MISURE_APP:
        ic.addPixmap(_file(f'icona-{lato}.png'))
    return ic


def vassoio(collegato: bool) -> QIcon:
    """Icona dell'area di notifica: a colori con il ricevitore collegato, grigia senza."""
    stato = 'attivo' if collegato else 'spento'
    ic = QIcon()
    for lato in (16, 32):
        ic.addPixmap(_file(f'vassoio-{stato}-{lato}.png'))
    return ic


def _pixmap_ico(lato: int) -> QPixmap:
    if lato in MISURE_APP:
        return _file(f'icona-{lato}.png')
    # Si riduce dalla misura disegnata subito sopra (mai ingrandire).
    origine = min(m for m in MISURE_APP if m > lato)
    return _file(f'icona-{origine}.png').scaled(lato, lato, Qt.IgnoreAspectRatio, Qt.SmoothTransformation)


def salva_ico(percorso: str) -> bool:
    """File .ico con tutte le misure di Windows (per PyInstaller e l'installer): ogni immagine e' un PNG."""
    immagini = []
    for lato in MISURE_ICO:
        pm = _pixmap_ico(lato)
        if pm.isNull() or pm.width() != lato:
            return False
        dati = QByteArray()
        buf = QBuffer(dati)
        buf.open(QIODevice.WriteOnly)
        pm.save(buf, 'PNG')
        buf.close()
        immagini.append((lato, bytes(dati)))
    testa = struct.pack('<HHH', 0, 1, len(immagini))
    elenco, corpo = b'', b''
    posizione = 6 + 16 * len(immagini)
    for lato, png in immagini:
        l = 0 if lato >= 256 else lato        # 0 = 256 nel formato ICO
        elenco += struct.pack('<BBBBHHII', l, l, 0, 0, 1, 32, len(png), posizione + len(corpo))
        corpo += png
    with open(percorso, 'wb') as f:
        f.write(testa + elenco + corpo)
    return True
