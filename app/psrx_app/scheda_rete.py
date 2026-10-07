"""
PS-RX app - scheda Rete: stato del WiFi, reti salvate (fino a 5) e Wake-on-LAN.
"""

from __future__ import annotations

import csv
import io
import re
import subprocess
import sys
from typing import List, Optional, Tuple

from PySide6.QtWidgets import (QAbstractItemView, QCheckBox, QDialog, QDialogButtonBox, QFormLayout, QGroupBox,
                               QHeaderView, QLabel, QLineEdit, QMenu, QPushButton, QTableWidget,
                               QTableWidgetItem, QToolButton, QVBoxLayout)

from psrx import protocollo as p

from .controlli import nota, riga
from .lavoratore import Istantanea
from .scheda_gamepad import Scheda

MAC_RE = re.compile(r'^([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}$')


def schede_di_rete() -> List[Tuple[str, str]]:
    """(nome, MAC) delle schede di rete di questo PC (Windows: getmac)."""
    if sys.platform != 'win32':
        return []
    try:
        uscita = subprocess.run(['getmac', '/fo', 'csv', '/v', '/nh'], capture_output=True, text=True,
                                timeout=5, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0)).stdout
    except (OSError, subprocess.SubprocessError):
        return []
    risultato = []
    for campi in csv.reader(io.StringIO(uscita)):
        if len(campi) >= 3 and MAC_RE.match(campi[2].strip()):
            risultato.append((f'{campi[0]} ({campi[1]})', campi[2].strip().replace('-', ':').upper()))
    return risultato


def _durata(ms: Optional[int]) -> str:
    if ms is None:
        return 'mai'
    s = ms // 1000
    if s < 60:
        return f'{s} s fa'
    if s < 3600:
        return f'{s // 60} min fa'
    return f'{s // 3600} h {s % 3600 // 60} min fa'


class DialogoRete(QDialog):
    def __init__(self, padre, rete: p.Rete):
        super().__init__(padre)
        self.setWindowTitle(f'Rete {rete.indice + 1}')
        self.rete = rete
        modulo = QFormLayout(self)
        self.ssid = QLineEdit(rete.ssid)
        self.ssid.setMaxLength(32)
        self.password = QLineEdit()
        self.password.setMaxLength(63)
        self.password.setEchoMode(QLineEdit.Password)
        if rete.ssid and rete.ha_password:
            self.password.setPlaceholderText('vuota = mantieni quella salvata')
        mostra = QCheckBox('Mostra')
        mostra.toggled.connect(lambda v: self.password.setEchoMode(QLineEdit.Normal if v else QLineEdit.Password))
        self.wpa3 = QCheckBox('WPA3')
        self.wpa3.setChecked(rete.wpa3)
        modulo.addRow('Nome della rete (SSID)', self.ssid)
        modulo.addRow('Password', riga(self.password, mostra, stretch=False))
        modulo.addRow('', self.wpa3)
        modulo.addRow(nota('WPA2 va bene per quasi tutte le reti: WPA3 solo se il router lo richiede. '
                           'Lascia la password vuota per una rete aperta.'))
        pulsanti = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel)
        pulsanti.accepted.connect(self._controlla)
        pulsanti.rejected.connect(self.reject)
        modulo.addRow(pulsanti)
        self.errore = QLabel('')
        self.errore.setStyleSheet('color: #c62828;')
        modulo.addRow(self.errore)

    def _controlla(self) -> None:
        ssid = self.ssid.text()
        pw = self.password.text()
        if not ssid:
            self.errore.setText('Serve il nome della rete')
        elif len(ssid.encode('utf-8')) > 32:
            self.errore.setText('Nome troppo lungo (massimo 32 byte)')
        elif pw and not 8 <= len(pw.encode('utf-8')) <= 63:
            self.errore.setText('La password WPA va da 8 a 63 caratteri')
        else:
            self.accept()

    def valori(self) -> tuple:
        ssid = self.ssid.text()
        pw = self.password.text()
        mantieni = not pw and ssid == self.rete.ssid and self.rete.ha_password
        return ssid, pw, self.wpa3.isChecked(), mantieni


class SchedaRete(Scheda):
    def __init__(self, finestra):
        super().__init__(finestra)
        gruppo = QGroupBox('WiFi del ricevitore')
        v = QVBoxLayout(gruppo)
        self.stato = QLabel('-')
        self.stato.setWordWrap(True)
        v.addWidget(self.stato)
        v.addWidget(nota('Il WiFi è acceso solo senza controller collegati, per mandare il Wake-on-LAN. Al '
                         'primo controller il ricevitore manda 3 pacchetti (entro 30 s al massimo), poi spegne '
                         'il WiFi: la radio resta tutta al Bluetooth. Si riaccende quando si spegne l\'ultimo.'))
        self.col.addWidget(gruppo)

        gruppo = QGroupBox(f'Reti salvate (fino a {p.RETI_MAX})')
        v = QVBoxLayout(gruppo)
        self.tabella = QTableWidget(p.RETI_MAX, 3)
        self.tabella.setHorizontalHeaderLabels(['Nome della rete', 'Sicurezza', 'Password'])
        self.tabella.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.tabella.verticalHeader().setVisible(False)
        self.tabella.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.tabella.setSelectionMode(QAbstractItemView.SingleSelection)
        self.tabella.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.tabella.cellDoubleClicked.connect(lambda r, _c: self._modifica(r))
        self.tabella.setFixedHeight(self.tabella.verticalHeader().defaultSectionSize() * (p.RETI_MAX + 1) + 6)
        v.addWidget(self.tabella)
        self.modifica = QPushButton('Aggiungi o modifica…')
        self.modifica.clicked.connect(lambda: self._modifica(self.tabella.currentRow()))
        self.elimina = QPushButton('Elimina')
        self.elimina.clicked.connect(self._elimina)
        v.addLayout(riga(self.modifica, self.elimina))
        v.addWidget(nota('Il ricevitore prova le reti in ordine e si collega alla prima che trova.'))
        self.col.addWidget(gruppo)

        gruppo = QGroupBox('Wake-on-LAN')
        v = QVBoxLayout(gruppo)
        self.globali = self.controlli_globali('rete', v)
        self.mac: List[QLineEdit] = []
        for i in range(2):
            e = QLineEdit()
            e.setPlaceholderText('AA:BB:CC:DD:EE:FF')
            e.setMaxLength(17)
            pulsante = QToolButton()
            pulsante.setText('Questo PC')
            pulsante.setPopupMode(QToolButton.InstantPopup)
            menu = QMenu(pulsante)
            menu.aboutToShow.connect(lambda m=menu, campo=e: self._riempi_menu(m, campo))
            pulsante.setMenu(menu)
            self.mac.append(e)
            v.addLayout(riga(QLabel(f'PC da svegliare {i + 1}'), e, pulsante))
        self.salva_wol = QPushButton('Salva')
        self.salva_wol.clicked.connect(self._salva_wol)
        self.prova_wol = QPushButton('Prova ora')
        self.prova_wol.setToolTip('Manda subito il pacchetto (serve il WiFi connesso, quindi nessun controller)')
        self.prova_wol.clicked.connect(lambda: self.esegui(lambda c: c.prova_wol(), 'Pacchetto Wake-on-LAN inviato'))
        v.addLayout(riga(self.salva_wol, self.prova_wol))
        v.addWidget(nota('Il MAC è quello della scheda di rete cablata del PC da accendere, con il Wake-on-LAN '
                         'attivo nel BIOS e in Windows. Perché il PC si accenda da spento, la porta USB del '
                         'ricevitore deve restare alimentata a PC spento (opzione del BIOS tipo "USB power in S5" '
                         'o "ErP" disattivato).'))
        self.col.addWidget(gruppo)
        self.col.addStretch(1)
        self._mac_modificati = False
        for e in self.mac:
            e.textEdited.connect(self._segna_mac)

    def _segna_mac(self, *_):
        self._mac_modificati = True

    def _riempi_menu(self, menu: QMenu, campo: QLineEdit) -> None:
        menu.clear()
        schede = schede_di_rete()
        if not schede:
            menu.addAction('Nessuna scheda di rete trovata').setEnabled(False)
        for nome, mac in schede:
            menu.addAction(f'{mac}  {nome}', lambda m=mac: (campo.setText(m), self._segna_mac()))

    def aggiorna(self, ist: Optional[Istantanea]) -> None:
        super().aggiorna(ist)
        attivo = ist is not None and ist.stato is not None and ist.reti is not None
        for w in (self.modifica, self.elimina, self.salva_wol, self.prova_wol, self.tabella, *self.mac):
            w.setEnabled(attivo)
        for ident, c in self.globali.items():
            c.mostra(ist.impostazioni.get(ident) if attivo else None)
        if not attivo:
            self.stato.setText('Ricevitore non collegato')
            return
        st = ist.stato
        testo = p.TESTO_RETE.get(st.rete_stato, '?')
        if st.rete_stato == p.RETE_CONNESSA:
            ssid = ist.reti.reti[st.rete_indice].ssid if 0 <= st.rete_indice < p.RETI_MAX else '?'
            testo = f'Connesso a <b>{ssid}</b> · IP {st.rete_ip} · segnale {st.rete_rssi} dBm'
        elif st.rete_stato == p.RETE_SPENTA and st.pad_connessi:
            testo = 'Spento: ci sono controller collegati, la radio è tutta al Bluetooth'
        if st.wol_finestra:
            testo += ' · invio del Wake-on-LAN in corso'
        testo += f'<br>Ultimo Wake-on-LAN: {_durata(st.ms_da_ultimo_wol)}'
        self.stato.setText(testo)
        for r, rete in enumerate(ist.reti.reti):
            valori = ([rete.ssid, 'WPA3' if rete.wpa3 else 'WPA2', 'salvata' if rete.ha_password else 'nessuna']
                      if rete.ssid else ['(vuota)', '', ''])
            for c, testo_cella in enumerate(valori):
                self.tabella.setItem(r, c, QTableWidgetItem(testo_cella))
        # Le modifiche non ancora salvate restano finché non si preme Salva.
        if not self._mac_modificati and not any(e.hasFocus() for e in self.mac):
            for e, mac in zip(self.mac, ist.reti.wol_mac):
                e.setText(mac)

    def _modifica(self, riga_: int) -> None:
        if self.ist is None or self.ist.reti is None:
            return
        if riga_ < 0:
            vuote = [r.indice for r in self.ist.reti.reti if not r.ssid]
            riga_ = vuote[0] if vuote else 0
        rete = self.ist.reti.reti[riga_]
        d = DialogoRete(self, rete)
        if d.exec() != QDialog.Accepted:
            return
        ssid, pw, wpa3, mantieni = d.valori()
        self.esegui(lambda c: c.salva_rete(riga_, ssid, pw, wpa3, mantieni), f'Rete {riga_ + 1} salvata: {ssid}')

    def _elimina(self) -> None:
        r = self.tabella.currentRow()
        if r < 0 or self.ist is None or not self.ist.reti.reti[r].ssid:
            return
        ssid = self.ist.reti.reti[r].ssid
        if self.finestra.conferma('Eliminare la rete?', f'{ssid} verrà tolta dal ricevitore.'):
            self.esegui(lambda c: c.cancella_rete(r), f'Rete {ssid} eliminata')

    def _salva_wol(self) -> None:
        valori = [e.text().strip() for e in self.mac]
        for v in valori:
            if v and not MAC_RE.match(v):
                self.finestra.messaggio(f'MAC non valido: {v}', errore=True)
                return
        valori = [v.replace('-', ':').upper() for v in valori]
        self._mac_modificati = False
        self.esegui(lambda c: c.destinazioni_wol(*valori), 'Destinazioni del Wake-on-LAN salvate')
