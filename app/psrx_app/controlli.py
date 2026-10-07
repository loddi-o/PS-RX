"""
PS-RX app - controlli costruiti dallo schema delle impostazioni (psrx/schema.py) e piccoli aiuti grafici.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtCore import Qt, Signal
from psrx.lingua import tr
from PySide6.QtWidgets import (QCheckBox, QComboBox, QFrame, QHBoxLayout, QLabel, QSpinBox, QVBoxLayout,
                               QWidget)


def nota(testo: str) -> QLabel:
    e = QLabel(testo)
    e.setWordWrap(True)
    e.setTextInteractionFlags(Qt.TextSelectableByMouse)
    e.setStyleSheet('color: palette(placeholder-text);')
    return e


def titolo(testo: str) -> QLabel:
    e = QLabel(testo)
    f = e.font()
    f.setBold(True)
    e.setFont(f)
    return e


def riga(*widget, stretch: bool = True) -> QHBoxLayout:
    r = QHBoxLayout()
    for w in widget:
        if isinstance(w, QWidget):
            r.addWidget(w)
        else:
            r.addLayout(w)
    if stretch:
        r.addStretch(1)
    return r


def separatore() -> QFrame:
    f = QFrame()
    f.setFrameShape(QFrame.HLine)
    f.setFrameShadow(QFrame.Sunken)
    return f


class Controllo(QWidget):
    """Una voce dello schema: casella, scelta o numero, con la nota sotto. 'cambiato' parte solo per
    le modifiche dell'utente, non per gli aggiornamenti dal ricevitore."""

    cambiato = Signal(int, int)   # (id, valore)

    def __init__(self, voce: dict, parent: Optional[QWidget] = None):
        super().__init__(parent)
        self.voce = voce
        self.valore: Optional[int] = None
        col = QVBoxLayout(self)
        col.setContentsMargins(0, 4, 0, 4)
        col.setSpacing(2)
        tipo = voce['tipo']
        if tipo == 'booleano':
            self.w = QCheckBox(tr(voce['titolo']))
            self.w.toggled.connect(lambda v: self._utente(1 if v else 0))
            col.addWidget(self.w)
        else:
            if tipo == 'scelta':
                self.w = QComboBox()
                for valore, testo in voce['opzioni']:
                    self.w.addItem(tr(testo), valore)
                self.w.activated.connect(lambda i: self._utente(self.w.itemData(i)))
            else:
                self.w = QSpinBox()
                self.w.setRange(voce['min'], voce['max'])
                self.w.setSingleStep(voce.get('passo', 1))
                self.w.setKeyboardTracking(False)
                self.w.valueChanged.connect(self._utente)
            col.addLayout(riga(QLabel(tr(voce['titolo'])), self.w))
        if voce.get('nota'):
            col.addWidget(nota(tr(voce['nota'])))
        self.setEnabled(False)

    def _utente(self, valore: int) -> None:
        if valore is None or valore == self.valore:
            return
        self.cambiato.emit(self.voce['id'], int(valore))

    def mostra(self, valore: Optional[int]) -> None:
        """Valore letto dal ricevitore (None = sconosciuto)."""
        self.valore = valore
        self.setEnabled(valore is not None)
        if valore is None:
            return
        self.w.blockSignals(True)
        try:
            if isinstance(self.w, QCheckBox):
                self.w.setChecked(bool(valore))
            elif isinstance(self.w, QComboBox):
                i = self.w.findData(valore)
                if i >= 0:
                    self.w.setCurrentIndex(i)
            else:
                self.w.setValue(valore)
        finally:
            self.w.blockSignals(False)
