"""
PS-RX app - scheda Gamepad: posti, controller abbinati e impostazioni per controller.
"""

from __future__ import annotations

from typing import Dict, List, Optional

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QGridLayout, QGroupBox, QHBoxLayout, QLabel, QLineEdit, QListWidget,
                               QListWidgetItem, QProgressBar, QPushButton, QScrollArea, QVBoxLayout, QWidget)

from psrx import protocollo as p
from psrx import schema

from .controlli import Controllo, nota, riga, titolo
from .lavoratore import Istantanea
from psrx.lingua import tr


class Scheda(QScrollArea):
    """Base delle schede: contenuto scorrevole e accesso alla finestra (richieste, messaggi)."""

    def __init__(self, finestra):
        super().__init__()
        self.finestra = finestra
        self.setWidgetResizable(True)
        self.setFrameShape(QScrollArea.NoFrame)
        interno = QWidget()
        self.col = QVBoxLayout(interno)
        self.col.setContentsMargins(16, 12, 16, 12)
        self.col.setSpacing(12)
        self.setWidget(interno)
        self.ist: Optional[Istantanea] = None

    def esegui(self, funzione, fatto_testo: str = '', fatto=None) -> None:
        self.finestra.esegui(funzione, fatto_testo, fatto)

    def controlli_globali(self, sezione: str, contenitore: QVBoxLayout) -> Dict[int, Controllo]:
        risultato = {}
        for voce in schema.IMPOSTAZIONI:
            if voce['sezione'] != sezione:
                continue
            c = Controllo(voce)
            c.cambiato.connect(self.finestra.imposta)
            contenitore.addWidget(c)
            risultato[voce['id']] = c
        return risultato

    def aggiorna(self, ist: Optional[Istantanea]) -> None:
        self.ist = ist


class Posto(QGroupBox):
    def __init__(self, indice: int, scheda: 'SchedaGamepad'):
        super().__init__(tr('Posto {0}', indice + 1))
        self.indice = indice
        col = QVBoxLayout(self)
        self.nome = titolo('-')
        self.dettagli = QLabel('')
        self.dettagli.setWordWrap(True)
        self.batteria = QProgressBar()
        self.batteria.setRange(0, 100)
        self.batteria.setTextVisible(True)
        self.spegni = QPushButton(tr('Spegni'))
        self.spegni.setToolTip(tr("Spegne il controller (l'abbinamento resta)"))
        self.spegni.clicked.connect(lambda: scheda.esegui(lambda c: c.spegni_pad(indice), tr('Posto {0} spento', indice + 1)))
        col.addWidget(self.nome)
        col.addWidget(self.dettagli)
        col.addLayout(riga(self.batteria, self.spegni, stretch=False))

    def mostra(self, pad: Optional[p.Pad], nome: str) -> None:
        if pad is None or not pad.connesso:
            self.nome.setText(tr('Libero'))
            self.dettagli.setText('')
            self.batteria.setVisible(False)
            self.spegni.setVisible(False)
            return
        self.nome.setText(nome or pad.mac)
        segnale = '-' if pad.rssi is None else f'{pad.rssi} dBm'
        self.dettagli.setText(tr('{0} · {1} report/s · {2} Hz · segnale {3}{4}', pad.nome_modello, pad.report_al_secondo, p.POLLING_HZ.get(pad.polling, '?'), segnale, tr(' · audio') if pad.audio else ''))
        self.batteria.setVisible(True)
        self.spegni.setVisible(True)
        if pad.batteria_valida:
            self.batteria.setValue(pad.batteria)
            self.batteria.setFormat(tr('Batteria %p%{0}', tr(' · in carica') if pad.in_carica else ''))
        else:
            self.batteria.setValue(0)
            self.batteria.setFormat(tr('Batteria sconosciuta'))


class SchedaGamepad(Scheda):
    def __init__(self, finestra):
        super().__init__(finestra)
        # posti
        griglia = QGridLayout()
        self.posti: List[Posto] = []
        for i in range(p.PAD_MAX):
            w = Posto(i, self)
            self.posti.append(w)
            griglia.addWidget(w, i // 2, i % 2)
        griglia.setColumnStretch(0, 1)
        griglia.setColumnStretch(1, 1)
        self.col.addLayout(griglia)

        # abbinamento
        self.abbina = QPushButton(tr('Abbina un nuovo controller'))
        self.abbina.clicked.connect(lambda: self.esegui(lambda c: c.abbina(), tr('Abbinamento aperto per 30 secondi')))
        self.spegni_tutti = QPushButton(tr('Spegni tutti'))
        self.spegni_tutti.clicked.connect(lambda: self.esegui(lambda c: c.spegni_pad(None), tr('Controller spenti')))
        self.stato_abbinamento = nota(tr('Per abbinare: premi il pulsante qui accanto (o un click sul BOOTSEL del Pico), poi tieni premuti Create + PS (DualSense) o Share + PS (DualShock 4) finché la luce lampeggia veloce.'))
        self.col.addLayout(riga(self.abbina, self.spegni_tutti))
        self.col.addWidget(self.stato_abbinamento)

        # controller abbinati e loro impostazioni
        gruppo = QGroupBox(tr('Controller abbinati'))
        orizz = QHBoxLayout(gruppo)
        self.lista = QListWidget()
        self.lista.setMinimumWidth(220)
        self.lista.currentItemChanged.connect(lambda *_: self._mostra_selezionato())
        orizz.addWidget(self.lista, 1)
        destra = QVBoxLayout()
        self.nome = QLineEdit()
        self.nome.setMaxLength(15)
        self.nome.setPlaceholderText(tr('Nome (facoltativo)'))
        self.rinomina = QPushButton(tr('Rinomina'))
        self.rinomina.clicked.connect(self._rinomina)
        self.nome.returnPressed.connect(self._rinomina)
        destra.addLayout(riga(self.nome, self.rinomina, stretch=False))
        self.controlli_pad: Dict[int, Controllo] = {}
        for voce in schema.IMPOSTAZIONI_PAD:
            c = Controllo(voce)
            c.cambiato.connect(self._imposta_pad)
            destra.addWidget(c)
            self.controlli_pad[voce['id']] = c
        self.dimentica = QPushButton(tr('Dimentica questo controller'))
        self.dimentica.clicked.connect(self._dimentica)
        destra.addLayout(riga(self.dimentica))
        destra.addStretch(1)
        orizz.addLayout(destra, 2)
        self.col.addWidget(gruppo)

        # impostazioni globali della sezione
        gruppo = QGroupBox(tr('Tutti i controller'))
        v = QVBoxLayout(gruppo)
        self.globali = self.controlli_globali('gamepad', v)
        self.col.addWidget(gruppo)
        self.col.addStretch(1)
        self._mostra_selezionato()

    # --- dati --------------------------------------------------------------------------------
    def _selezionato(self) -> Optional[p.Abbinato]:
        voce = self.lista.currentItem()
        if voce is None or self.ist is None:
            return None
        mac = voce.data(Qt.UserRole)
        return next((a for a in self.ist.abbinati if a.mac == mac), None)

    def aggiorna(self, ist: Optional[Istantanea]) -> None:
        super().aggiorna(ist)
        attivo = ist is not None and ist.stato is not None
        for w in (self.abbina, self.spegni_tutti):
            w.setEnabled(attivo)
        nomi = {a.mac: a.nome for a in ist.abbinati} if attivo else {}
        for i, w in enumerate(self.posti):
            w.mostra(ist.stato.pad[i] if attivo else None, nomi.get(ist.stato.pad[i].mac, '') if attivo else '')
        if attivo and ist.stato.finestra_abbinamento:
            self.abbina.setText(tr('Abbinamento in corso…'))
        else:
            self.abbina.setText(tr('Abbina un nuovo controller'))

        # lista, tenendo la selezione
        scelto = self.lista.currentItem().data(Qt.UserRole) if self.lista.currentItem() else None
        self.lista.blockSignals(True)
        self.lista.clear()
        for a in (ist.abbinati if attivo else []):
            testo = a.nome or a.mac
            if a.posto is not None:
                testo += tr('  ·  posto {0}', a.posto + 1)
            voce = QListWidgetItem(testo)
            voce.setData(Qt.UserRole, a.mac)
            voce.setToolTip(a.mac)
            self.lista.addItem(voce)
            if a.mac == scelto:
                self.lista.setCurrentItem(voce)
        if self.lista.currentItem() is None and self.lista.count():
            self.lista.setCurrentRow(0)
        self.lista.blockSignals(False)
        self._mostra_selezionato()

        for ident, c in self.globali.items():
            c.mostra(ist.impostazioni.get(ident) if attivo else None)

    def _mostra_selezionato(self) -> None:
        a = self._selezionato()
        for w in (self.nome, self.rinomina, self.dimentica):
            w.setEnabled(a is not None)
        if not self.nome.hasFocus():
            self.nome.setText(a.nome if a else '')
        for voce in schema.IMPOSTAZIONI_PAD:
            valore = None if a is None else int(getattr(a, voce['chiave']))
            self.controlli_pad[voce['id']].mostra(valore)
        if a is not None:
            self.controlli_pad[p.PAD_INVERTI_SCORRIMENTO].setEnabled(a.trackpad)
            self.controlli_pad[p.PAD_MICROFONO].setEnabled(a.audio)   # senza audio il microfono non c'e'


    # --- azioni ------------------------------------------------------------------------------
    def _imposta_pad(self, ident: int, valore: int) -> None:
        a = self._selezionato()
        if a is None:
            return
        voce = schema.PAD_PER_ID[ident]
        testo = f'{a.nome or a.mac}: {tr(voce["titolo"])} = {schema.testo_valore(voce, valore)}'
        if ident in (p.PAD_TRACKPAD, p.PAD_AUDIO):
            testo += tr(" (il ricevitore si ricollega all'USB per cambiare forma)")
        self.esegui(lambda c: c.imposta_pad(a.mac, ident, valore), testo)

    def _rinomina(self) -> None:
        a = self._selezionato()
        if a is None:
            return
        nome = self.nome.text().strip()
        self.nome.clearFocus()
        self.esegui(lambda c: c.rinomina(a.mac, nome), tr('Nome salvato: {0}', nome or a.mac))

    def _dimentica(self) -> None:
        a = self._selezionato()
        if a is None:
            return
        if not self.finestra.conferma(tr('Dimenticare il controller?'),
                                      tr('{0} va abbinato di nuovo per usarlo. Le sue impostazioni si perdono.', a.nome or a.mac)):
            return
        self.esegui(lambda c: c.dimentica(a.mac), tr('{0} dimenticato', a.nome or a.mac))
