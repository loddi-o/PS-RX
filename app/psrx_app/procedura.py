"""
PS-RX app - procedura guidata "Prepara un nuovo ricevitore": firmware su un Pico 2 W nuovo (o in BOOTSEL),
come lo riceve chi lo compra: si collega, l'app lo riconosce, scarica il firmware piu' recente da GitHub (o
usa quello incluso nell'app se non c'e' internet), lo copia nel Pico, aspetta che diventi un PS-RX e
propone l'abbinamento del primo controller.
"""

from __future__ import annotations

import threading
import time
from typing import Optional

from PySide6.QtCore import QTimer, Signal
from PySide6.QtWidgets import QDialog, QLabel, QProgressBar, QPushButton, QVBoxLayout

from psrx import aggiornamenti as ag
from psrx import pico_nuovo
from psrx.firmware import FirmwareNonValido, leggi as leggi_firmware

from .aggiorna import CARTELLA
from .controlli import nota, riga, titolo

ATTESA_AVVIO_S = 60


class Procedura(QDialog):
    _passo = Signal(str, int)        # testo, percentuale (-1 = barra nascosta)
    _finito = Signal(str)            # errore ('' = copia riuscita)

    def __init__(self, finestra):
        super().__init__(finestra)
        self.finestra = finestra
        self.setWindowTitle('Prepara un nuovo ricevitore')
        self.setMinimumWidth(520)
        col = QVBoxLayout(self)
        self.titolo = titolo('1. Collega il Raspberry Pi Pico 2 W')
        self.testo = QLabel()
        self.testo.setWordWrap(True)
        self.barra = QProgressBar()
        self.barra.setVisible(False)
        col.addWidget(self.titolo)
        col.addWidget(self.testo)
        col.addWidget(self.barra)
        col.addWidget(nota('Serve un Pico 2 W (con il WiFi/Bluetooth): il Pico 2 senza "W" non ha la radio. Tutto '
                           'quello che c\'era sul Pico viene sostituito.'))
        self.riflasha = QPushButton('Riflasha il ricevitore PS-RX collegato')
        self.riflasha.clicked.connect(self._riflasha)
        self.abbina = QPushButton('Abbina il primo controller')
        self.abbina.clicked.connect(self._abbina)
        self.chiudi = QPushButton('Chiudi')
        self.chiudi.clicked.connect(self.reject)
        col.addLayout(riga(self.riflasha, self.abbina, self.chiudi))
        self.abbina.setVisible(False)

        self.stato = 'attesa'            # attesa -> copia -> avvio -> fatto / errore
        self.t_copia = 0.0
        self.unita: Optional[str] = None
        self._passo.connect(self._su_passo)
        self._finito.connect(self._su_finito)
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._controlla)
        self.timer.start(700)
        self._attesa()
        self._controlla()

    # --- passi ---------------------------------------------------------------------------------
    def _attesa(self) -> None:
        self.stato = 'attesa'
        self.titolo.setText('1. Collega il Raspberry Pi Pico 2 W')
        self.testo.setText('Collega il Pico a una porta USB di questo PC.<br>'
                           '• <b>Pico nuovo</b>: si presenta da solo come chiavetta "RP2350".<br>'
                           '• <b>Pico già usato</b>: tieni premuto il tasto <b>BOOTSEL</b> mentre colleghi il cavo, '
                           'poi rilascialo.<br><br>In attesa del Pico…')

    def _controlla(self) -> None:
        collegato = self.finestra.ist is not None and self.finestra.ist.info is not None
        if self.stato == 'attesa':
            self.riflasha.setVisible(collegato)
            unita = pico_nuovo.trova()
            if unita:
                self._avvia_copia(unita[0])
        elif self.stato == 'avvio':
            if collegato:
                self.stato = 'fatto'
                self.titolo.setText('Fatto: il ricevitore è pronto')
                self.testo.setText(f'PS-RX {self.finestra.ist.info.versione} è in funzione.<br>Ora abbina un '
                                   'controller: premi il pulsante qui sotto, poi tieni premuti <b>Create + PS</b> '
                                   '(DualSense) o <b>Share + PS</b> (DualShock 4) finché la luce lampeggia veloce.')
                self.abbina.setVisible(True)
                self.chiudi.setText('Fine')
            elif time.monotonic() - self.t_copia > ATTESA_AVVIO_S:
                self._errore('Il Pico non si è presentato come PS-RX. È un Pico 2 <b>W</b>? Prova a scollegarlo e '
                             'ricollegarlo; se non parte, ripeti la procedura.')

    def _avvia_copia(self, unita: str) -> None:
        self.stato = 'copia'
        self.unita = unita
        self.riflasha.setVisible(False)
        self.chiudi.setEnabled(False)
        self.titolo.setText('2. Installazione del firmware')
        self._su_passo('Ricerca del firmware più recente su GitHub…', -1)

        def lavoro():
            try:
                percorso, origine = self._prendi_firmware()
                img = leggi_firmware(percorso)   # stessi controlli dell'aggiornamento dall'app
                self._passo.emit(f'Copia di PS-RX {img.versione or ""} ({origine}) nel Pico: non scollegarlo…', 0)
                pico_nuovo.scrivi_uf2(unita, percorso,
                                      lambda fatti, totali: self._passo.emit('', int(fatti * 100 / max(totali, 1))))
                self._finito.emit('')
            except (OSError, FirmwareNonValido, ag.ErroreAggiornamento) as e:
                self._finito.emit(str(e))

        threading.Thread(target=lavoro, name='ps-rx-pico-nuovo', daemon=True).start()

    def _prendi_firmware(self):
        """Firmware .uf2: l'ultima release su GitHub, altrimenti quello incluso nell'app."""
        try:
            rel = ag.ultima_release()
            f = rel.file.get('firmware')
            if f is not None:
                return ag.scarica(f, CARTELLA), f'versione {rel.versione} da GitHub'
        except ag.ErroreAggiornamento:
            pass
        incluso = pico_nuovo.firmware_incluso()
        if incluso:
            return incluso, 'incluso nell\'app, GitHub non raggiungibile'
        raise ag.ErroreAggiornamento('GitHub non raggiungibile e nessun firmware incluso nell\'app')

    def _su_passo(self, testo: str, percento: int) -> None:
        if testo:
            self.testo.setText(testo)
        self.barra.setVisible(percento >= 0)
        if percento >= 0:
            self.barra.setValue(percento)

    def _su_finito(self, errore: str) -> None:
        self.chiudi.setEnabled(True)
        if errore:
            self._errore(f'Installazione non riuscita: {errore}')
            return
        self.stato = 'avvio'
        self.t_copia = time.monotonic()
        self.barra.setVisible(False)
        self.titolo.setText('3. Avvio del ricevitore')
        self.testo.setText('Il Pico ha ricevuto il firmware e si sta riavviando come PS-RX (pochi secondi; la prima '
                           'volta Windows prepara anche il dispositivo).')

    def _errore(self, testo: str) -> None:
        self.stato = 'errore'
        self.titolo.setText('Qualcosa non è andato')
        self.testo.setText(testo + '<br><br>Ricollega il Pico (tenendo BOOTSEL) per riprovare.')
        self.barra.setVisible(False)
        self.chiudi.setEnabled(True)
        QTimer.singleShot(4000, self._riprova_se_presente)

    def _riprova_se_presente(self) -> None:
        if self.stato == 'errore':
            self._attesa()

    # --- azioni ---------------------------------------------------------------------------------
    def _riflasha(self) -> None:
        if self.finestra.conferma('Riflashare il ricevitore?',
                                  'Il ricevitore si riavvia in modalità BOOTSEL e riceve di nuovo il firmware più '
                                  'recente. Impostazioni e abbinamenti restano. Servono i controller spenti.'):
            self.finestra.esegui(lambda c: c.bootsel_ora(), 'Ricevitore in modalità BOOTSEL')

    def _abbina(self) -> None:
        self.finestra.esegui(lambda c: c.abbina(), 'Abbinamento aperto per 30 secondi')
        self.abbina.setEnabled(False)

    def done(self, r: int) -> None:
        self.timer.stop()
        super().done(r)
