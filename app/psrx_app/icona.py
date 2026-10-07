"""
PS-RX app - icona disegnata al volo (nessun file binario nella repo): quadrato arrotondato con "RX".
Il colore cambia con lo stato: blu con il ricevitore collegato, grigio senza.
"""

from __future__ import annotations

from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QColor, QFont, QIcon, QPainter, QPixmap

BLU = QColor('#1f6feb')
GRIGIO = QColor('#8b949e')


def pixmap(lato: int, colore: QColor = BLU) -> QPixmap:
    pm = QPixmap(lato, lato)
    pm.fill(Qt.transparent)
    pt = QPainter(pm)
    pt.setRenderHint(QPainter.Antialiasing)
    pt.setPen(Qt.NoPen)
    pt.setBrush(colore)
    margine = lato * 0.04
    pt.drawRoundedRect(QRectF(margine, margine, lato - 2 * margine, lato - 2 * margine), lato * 0.22, lato * 0.22)
    f = QFont('Segoe UI')
    f.setBold(True)
    f.setPixelSize(int(lato * 0.46))
    pt.setFont(f)
    pt.setPen(QColor('white'))
    pt.drawText(QRectF(0, 0, lato, lato), Qt.AlignCenter, 'RX')
    pt.end()
    return pm


def icona(colore: QColor = BLU) -> QIcon:
    ic = QIcon()
    for lato in (16, 20, 24, 32, 48, 64, 128, 256):
        ic.addPixmap(pixmap(lato, colore))
    return ic


def salva_ico(percorso: str) -> bool:
    """Per PyInstaller (icona dell'exe)."""
    return pixmap(256).save(percorso, 'ICO')
