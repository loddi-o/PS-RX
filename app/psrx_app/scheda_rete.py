"""
PS-RX app - scheda Rete: stato del WiFi, reti salvate (fino a 5) e Wake-on-LAN.
"""

from __future__ import annotations

import csv
import io
import re
import subprocess
import sys
import time
from typing import List, Optional, Tuple

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import (QAbstractItemView, QCheckBox, QDialog, QDialogButtonBox, QFormLayout, QGroupBox,
                               QHeaderView, QLabel, QLineEdit, QMenu, QPushButton, QTableWidget,
                               QTableWidgetItem, QToolButton, QVBoxLayout)

from psrx import protocollo as p

from .controlli import nota, riga
from .lavoratore import Istantanea, testo_errore
from .scheda_gamepad import Scheda
from psrx.lingua import tr

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
        return tr('mai')
    s = ms // 1000
    if s < 60:
        return tr('{0} s fa', s)
    if s < 3600:
        return tr('{0} min fa', s // 60)
    return tr('{0} h {1} min fa', s // 3600, s % 3600 // 60)


class DialogoRete(QDialog):
    def __init__(self, padre, rete: p.Rete, ssid_proposto: str = '', aperta: bool = False):
        super().__init__(padre)
        self.setWindowTitle(tr('Rete {0}', rete.indice + 1))
        self.rete = rete
        modulo = QFormLayout(self)
        self.ssid = QLineEdit(ssid_proposto or rete.ssid)
        self.ssid.setMaxLength(32)
        self.password = QLineEdit()
        self.password.setMaxLength(63)
        self.password.setEchoMode(QLineEdit.Password)
        if rete.ssid and rete.ha_password:
            self.password.setPlaceholderText(tr('vuota = mantieni quella salvata'))
        mostra = QCheckBox(tr('Mostra'))
        mostra.toggled.connect(lambda v: self.password.setEchoMode(QLineEdit.Normal if v else QLineEdit.Password))
        self.wpa3 = QCheckBox(tr('WPA3'))
        self.wpa3.setChecked(rete.wpa3 and not ssid_proposto)
        if ssid_proposto:
            self.password.setFocus()
            if aperta:
                self.password.setPlaceholderText(tr('rete aperta: nessuna password'))
        modulo.addRow(tr('Nome della rete (SSID)'), self.ssid)
        modulo.addRow(tr('Password'), riga(self.password, mostra, stretch=False))
        modulo.addRow('', self.wpa3)
        modulo.addRow(nota(tr('<b>Solo reti a 2,4 GHz</b>: il Pico 2 W non vede le reti a 5 GHz. Se il router usa lo stesso nome per 2,4 e 5 GHz va bene, si collega da solo ai 2,4; se ha due nomi (per esempio "Casa" e "Casa_5G"), scegli quello a 2,4 GHz.')))
        modulo.addRow(nota(tr('WPA2 va bene per quasi tutte le reti: WPA3 solo se il router lo richiede. Lascia la password vuota per una rete aperta. Dopo il salvataggio parte una prova.')))
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
            self.errore.setText(tr('Serve il nome della rete'))
        elif len(ssid.encode('utf-8')) > 32:
            self.errore.setText(tr('Nome troppo lungo (massimo 32 byte)'))
        elif pw and not 8 <= len(pw.encode('utf-8')) <= 63:
            self.errore.setText(tr('La password WPA va da 8 a 63 caratteri'))
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
        gruppo = QGroupBox(tr('WiFi del ricevitore'))
        v = QVBoxLayout(gruppo)
        self.stato = QLabel('-')
        self.stato.setWordWrap(True)
        v.addWidget(self.stato)
        v.addWidget(nota(tr("Il WiFi è acceso solo senza controller collegati, per mandare il Wake-on-LAN. Al primo controller il ricevitore manda 3 pacchetti (entro 30 s al massimo), poi spegne il WiFi: la radio resta tutta al Bluetooth. Si riaccende quando si spegne l'ultimo. Il Pico 2 W usa solo reti a <b>2,4 GHz</b>.")))
        self.col.addWidget(gruppo)

        gruppo = QGroupBox(tr('Reti salvate (fino a {0})', p.RETI_MAX))
        v = QVBoxLayout(gruppo)
        self.tabella = QTableWidget(p.RETI_MAX, 3)
        self.tabella.setHorizontalHeaderLabels([tr('Nome della rete'), tr('Sicurezza'), tr('Password')])
        self.tabella.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.tabella.verticalHeader().setVisible(False)
        self.tabella.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.tabella.setSelectionMode(QAbstractItemView.SingleSelection)
        self.tabella.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.tabella.cellDoubleClicked.connect(lambda r, _c: self._modifica(r))
        self.tabella.setFixedHeight(self.tabella.verticalHeader().defaultSectionSize() * (p.RETI_MAX + 1) + 6)
        v.addWidget(self.tabella)
        self.modifica = QPushButton(tr('Aggiungi o modifica…'))
        self.modifica.clicked.connect(lambda: self._modifica(self.tabella.currentRow()))
        self.elimina = QPushButton(tr('Elimina'))
        self.elimina.clicked.connect(self._elimina)
        self.prova = QPushButton(tr('Prova la rete'))
        self.prova.setToolTip(tr('Controlla password, indirizzo dal router e internet (solo senza controller collegati)'))
        self.prova.clicked.connect(lambda: self._avvia_prova(self.tabella.currentRow()))
        v.addLayout(riga(self.modifica, self.elimina, self.prova))
        self.esito_prova = QLabel('')
        self.esito_prova.setWordWrap(True)
        v.addWidget(self.esito_prova)
        v.addWidget(nota(tr('Il ricevitore prova le reti in ordine e si collega alla prima che trova.')))
        self.col.addWidget(gruppo)

        gruppo = QGroupBox(tr('Reti rilevate'))
        v = QVBoxLayout(gruppo)
        self.cerca = QPushButton(tr('Cerca reti'))
        self.cerca.clicked.connect(self._cerca_reti)
        self.aggiungi_vista = QPushButton(tr('Usa questa rete…'))
        self.aggiungi_vista.clicked.connect(lambda: self._usa_vista(self.viste.currentRow()))
        self.aggiungi_vista.setEnabled(False)
        v.addLayout(riga(self.cerca, self.aggiungi_vista))
        self.viste = QTableWidget(0, 4)
        self.viste.setHorizontalHeaderLabels([tr('Nome della rete'), tr('Segnale'), tr('Canale'), tr('Sicurezza')])
        self.viste.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.viste.verticalHeader().setVisible(False)
        self.viste.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.viste.setSelectionMode(QAbstractItemView.SingleSelection)
        self.viste.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.viste.cellDoubleClicked.connect(lambda r, _c: self._usa_vista(r))
        self.viste.itemSelectionChanged.connect(lambda: self.aggiungi_vista.setEnabled(self.viste.currentRow() >= 0))
        self.viste.setVisible(False)
        v.addWidget(self.viste)
        self.esito_cerca = QLabel('')
        self.esito_cerca.setWordWrap(True)
        v.addWidget(self.esito_cerca)
        v.addWidget(nota(tr('Le reti che il ricevitore vede da dove si trova (solo 2,4 GHz; le reti nascoste non compaiono). Doppio clic su una rete per salvarla. Solo senza controller collegati.')))
        self.col.addWidget(gruppo)
        self._reti_viste = []
        self._timer_cerca = QTimer(self)
        self._timer_cerca.timeout.connect(self._leggi_viste)
        self._t_cerca = 0.0

        gruppo = QGroupBox('Wake-on-LAN')
        v = QVBoxLayout(gruppo)
        self.globali = self.controlli_globali('rete', v)
        self.mac: List[QLineEdit] = []
        for i in range(2):
            e = QLineEdit()
            e.setPlaceholderText(tr('AA:BB:CC:DD:EE:FF'))
            e.setMaxLength(17)
            pulsante = QToolButton()
            pulsante.setText(tr('Questo PC'))
            pulsante.setPopupMode(QToolButton.InstantPopup)
            menu = QMenu(pulsante)
            menu.aboutToShow.connect(lambda m=menu, campo=e: self._riempi_menu(m, campo))
            pulsante.setMenu(menu)
            self.mac.append(e)
            v.addLayout(riga(QLabel(tr('PC da svegliare {0}', i + 1)), e, pulsante))
        self.salva_wol = QPushButton(tr('Salva'))
        self.salva_wol.clicked.connect(self._salva_wol)
        self.prova_wol = QPushButton(tr('Prova ora'))
        self.prova_wol.setToolTip(tr('Manda subito il pacchetto (serve il WiFi connesso, quindi nessun controller)'))
        self.prova_wol.clicked.connect(lambda: self.esegui(lambda c: c.prova_wol(), tr('Pacchetto Wake-on-LAN inviato')))
        v.addLayout(riga(self.salva_wol, self.prova_wol))
        v.addWidget(nota(tr('Il MAC è quello della scheda di rete cablata del PC da accendere, con il Wake-on-LAN attivo nel BIOS e in Windows. Perché il PC si accenda da spento, la porta USB del ricevitore deve restare alimentata a PC spento (opzione del BIOS tipo "USB power in S5" o "ErP" disattivato).')))
        self.col.addWidget(gruppo)
        self.col.addStretch(1)
        self._mac_modificati = False
        for e in self.mac:
            e.textEdited.connect(self._segna_mac)
        self._timer_prova = QTimer(self)
        self._timer_prova.timeout.connect(self._leggi_prova)
        self._t_prova = 0

    def _segna_mac(self, *_):
        self._mac_modificati = True

    def _riempi_menu(self, menu: QMenu, campo: QLineEdit) -> None:
        menu.clear()
        schede = schede_di_rete()
        if not schede:
            menu.addAction(tr('Nessuna scheda di rete trovata')).setEnabled(False)
        for nome, mac in schede:
            menu.addAction(f'{mac}  {nome}', lambda m=mac: (campo.setText(m), self._segna_mac()))

    def aggiorna(self, ist: Optional[Istantanea]) -> None:
        super().aggiorna(ist)
        attivo = ist is not None and ist.stato is not None and ist.reti is not None
        for w in (self.modifica, self.elimina, self.salva_wol, self.prova_wol, self.tabella, *self.mac):
            w.setEnabled(attivo)
        senza_pad = attivo and ist.stato.pad_connessi == 0
        self.prova.setEnabled(senza_pad and not self._timer_prova.isActive())
        self.cerca.setEnabled(senza_pad and not self._timer_cerca.isActive())
        self.cerca.setToolTip('' if senza_pad else tr('Spegni i controller: con un controller collegato il WiFi è spento'))
        self.prova.setToolTip(tr('Controlla password, indirizzo dal router e internet') if senza_pad else
                              tr('Spegni i controller: con un controller collegato il WiFi è spento'))
        for ident, c in self.globali.items():
            c.mostra(ist.impostazioni.get(ident) if attivo else None)
        if not attivo:
            self.stato.setText(tr('Ricevitore non collegato'))
            return
        st = ist.stato
        testo = p.TESTO_RETE.get(st.rete_stato, '?')
        if st.rete_stato == p.RETE_CONNESSA:
            ssid = ist.reti.reti[st.rete_indice].ssid if 0 <= st.rete_indice < p.RETI_MAX else '?'
            testo = tr('Connesso a <b>{0}</b> · IP {1} · segnale {2} dBm', ssid, st.rete_ip, st.rete_rssi)
        elif st.rete_stato == p.RETE_SPENTA and st.pad_connessi:
            testo = tr('Spento: ci sono controller collegati, la radio è tutta al Bluetooth')
        if st.wol_finestra:
            testo += tr(' · invio del Wake-on-LAN in corso')
        testo += tr('<br>Ultimo Wake-on-LAN: {0}', _durata(st.ms_da_ultimo_wol))
        self.stato.setText(testo)
        for r, rete in enumerate(ist.reti.reti):
            valori = ([rete.ssid, tr('WPA3') if rete.wpa3 else tr('WPA2'), tr('salvata') if rete.ha_password else tr('nessuna')]
                      if rete.ssid else [tr('(vuota)'), '', ''])
            for c, testo_cella in enumerate(valori):
                self.tabella.setItem(r, c, QTableWidgetItem(testo_cella))
        # Le modifiche non ancora salvate restano finché non si preme Salva.
        if not self._mac_modificati and not any(e.hasFocus() for e in self.mac):
            for e, mac in zip(self.mac, ist.reti.wol_mac):
                e.setText(mac)

    # --- reti rilevate --------------------------------------------------------------------------
    def _cerca_reti(self) -> None:
        self.cerca.setEnabled(False)
        self.esito_cerca.setText(tr('Ricerca delle reti (qualche secondo)…'))

        def avviata(_):
            self._t_cerca = time.monotonic()
            self._timer_cerca.start(700)

        def errore(e):
            self.cerca.setEnabled(True)
            self.esito_cerca.setText(tr('Ricerca non avviata: {0}', testo_errore(e)))

        self.finestra.ponte.esegui(lambda c: c.cerca_reti(), avviata, errore)

    def _leggi_viste(self) -> None:
        if time.monotonic() - self._t_cerca > 25:
            self._timer_cerca.stop()
            self.cerca.setEnabled(True)
            self.esito_cerca.setText(tr('La ricerca non ha dato risposta in tempo: riprova.'))
            return
        self.finestra.ponte.esegui(lambda c: c.reti_viste(), self._mostra_viste)

    def _mostra_viste(self, viste) -> None:
        if not viste.finita:
            return
        self._timer_cerca.stop()
        self.cerca.setEnabled(True)
        if viste.stato == p.SCAN_ANNULLATA:
            self.esito_cerca.setText(tr('Ricerca interrotta (si è collegato un controller?): riprova.'))
            return
        salvate = {r.ssid for r in self.ist.reti.reti} if self.ist and self.ist.reti else set()
        self._reti_viste = viste.reti
        self.viste.setRowCount(len(viste.reti))
        for i, r in enumerate(viste.reti):
            nome = r.ssid + (tr('  (salvata)') if r.ssid in salvate else '')
            valori = [nome, f'{"▮" * r.tacche}{"▯" * (4 - r.tacche)}  {r.rssi} dBm', str(r.canale),
                      tr('aperta') if r.aperta else tr('protetta')]
            for c, testo in enumerate(valori):
                self.viste.setItem(i, c, QTableWidgetItem(testo))
        self.viste.setVisible(bool(viste.reti))
        self.viste.setFixedHeight(self.viste.verticalHeader().defaultSectionSize() * (min(len(viste.reti), 8) + 1) + 6)
        self.esito_cerca.setText(tr('{0} reti rilevate.', len(viste.reti)) if viste.reti else
                                 tr('Nessuna rete rilevata: il router è acceso e trasmette a 2,4 GHz?'))

    def _usa_vista(self, riga_: int) -> None:
        if riga_ < 0 or riga_ >= len(self._reti_viste) or self.ist is None or self.ist.reti is None:
            return
        vista = self._reti_viste[riga_]
        esistente = next((r for r in self.ist.reti.reti if r.ssid == vista.ssid), None)
        vuota = next((r for r in self.ist.reti.reti if not r.ssid), None)
        posto = esistente or vuota
        if posto is None:
            self.finestra.messaggio(tr('Già {0} reti salvate: eliminane una per aggiungere "{1}"', p.RETI_MAX, vista.ssid),
                                    errore=True)
            return
        self._modifica(posto.indice, vista.ssid, vista.aperta)

    def _modifica(self, riga_: int, ssid_proposto: str = '', aperta: bool = False) -> None:
        if self.ist is None or self.ist.reti is None:
            return
        if riga_ < 0:
            vuote = [r.indice for r in self.ist.reti.reti if not r.ssid]
            riga_ = vuote[0] if vuote else 0
        rete = self.ist.reti.reti[riga_]
        d = DialogoRete(self, rete, ssid_proposto, aperta)
        if d.exec() != QDialog.Accepted:
            return
        ssid, pw, wpa3, mantieni = d.valori()
        senza_pad = self.ist.stato is not None and self.ist.stato.pad_connessi == 0
        # Dopo il salvataggio, prova subito la rete (se non ci sono controller: con un controller il WiFi e' spento).
        self.esegui(lambda c: c.salva_rete(riga_, ssid, pw, wpa3, mantieni), tr('Rete {0} salvata: {1}', riga_ + 1, ssid),
                    (lambda _: self._avvia_prova(riga_)) if senza_pad else None)

    # --- prova della rete -----------------------------------------------------------------------
    def _avvia_prova(self, indice: int) -> None:
        if self.ist is None or self.ist.reti is None or indice < 0 or not self.ist.reti.reti[indice].ssid:
            self.finestra.messaggio(tr('Scegli una rete salvata da provare'), errore=True)
            return
        ssid = self.ist.reti.reti[indice].ssid
        self.esito_prova.setStyleSheet('')
        self.esito_prova.setText(tr('Prova di "{0}": collegamento alla rete…', ssid))
        self.prova.setEnabled(False)

        def avviata(_):
            self._t_prova = time.monotonic()
            self._timer_prova.start(700)

        def errore(e):
            self.prova.setEnabled(True)
            self.esito_prova.setText(tr('Prova non avviata: {0}', testo_errore(e)))

        self.finestra.ponte.esegui(lambda c: c.prova_rete(indice), avviata, errore)

    def _leggi_prova(self) -> None:
        if time.monotonic() - self._t_prova > 70:
            self._timer_prova.stop()
            self.esito_prova.setText(tr('La prova non ha dato risposta in tempo: riprova.'))
            self.prova.setEnabled(True)
            return
        self.finestra.ponte.esegui(lambda c: c.esito_prova_rete(), self._mostra_prova)

    def _mostra_prova(self, esito) -> None:
        if esito.fase == p.PROVA_NESSUNA:
            return
        ssid = ''
        if self.ist and self.ist.reti and 0 <= esito.rete < p.RETI_MAX:
            ssid = self.ist.reti.reti[esito.rete].ssid
        self.esito_prova.setText(tr('Prova di "{0}": {1}', ssid, esito.descrizione))
        if esito.finita:
            self._timer_prova.stop()
            self.prova.setEnabled(True)
            ok = esito.esito == p.ESITO_OK
            self.esito_prova.setStyleSheet('' if ok else 'color: #c62828;')
            self.finestra.lav_aggiorna()

    def _elimina(self) -> None:
        r = self.tabella.currentRow()
        if r < 0 or self.ist is None or not self.ist.reti.reti[r].ssid:
            return
        ssid = self.ist.reti.reti[r].ssid
        if self.finestra.conferma(tr('Eliminare la rete?'), tr('{0} verrà tolta dal ricevitore.', ssid)):
            self.esegui(lambda c: c.cancella_rete(r), tr('Rete {0} eliminata', ssid))

    def _salva_wol(self) -> None:
        valori = [e.text().strip() for e in self.mac]
        for v in valori:
            if v and not MAC_RE.match(v):
                self.finestra.messaggio(tr('MAC non valido: {0}', v), errore=True)
                return
        valori = [v.replace('-', ':').upper() for v in valori]
        self._mac_modificati = False
        self.esegui(lambda c: c.destinazioni_wol(*valori), tr('Destinazioni del Wake-on-LAN salvate'))
