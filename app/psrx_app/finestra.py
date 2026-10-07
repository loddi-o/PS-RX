"""
PS-RX app - finestra principale (Gamepad / Rete / Sistema) e icona nell'area di notifica.
"""

from __future__ import annotations

from typing import Callable, Optional

from PySide6.QtCore import QTimer
from PySide6.QtGui import QAction, QCloseEvent
from PySide6.QtWidgets import (QApplication, QHBoxLayout, QLabel, QMainWindow, QMenu, QMessageBox, QPushButton,
                               QSystemTrayIcon, QTabWidget, QVBoxLayout, QWidget)

from psrx import pico_nuovo
from psrx import protocollo as p
from psrx import schema

from . import aggiorna, icona, opzioni
from .lavoratore import Istantanea, Ponte, testo_errore
from .scheda_gamepad import SchedaGamepad
from .scheda_rete import SchedaRete
from .scheda_sistema import SchedaSistema


class Finestra(QMainWindow):
    def __init__(self, ponte: Ponte, simulato: bool = False):
        super().__init__()
        self.ponte = ponte
        self.ist: Optional[Istantanea] = None
        self.server = None   # istanza unica (__main__): si chiude prima di riavviare l'app aggiornata
        self.aggiornatore = aggiorna.Aggiornatore()
        self.aggiornatore.esito.connect(self._su_esito_aggiornamenti)
        self._ricerca_automatica = False
        self.procedura = None
        self._bootsel_visto = False   # procedura gia' proposta per il Pico in BOOTSEL collegato adesso
        self.setWindowTitle('PS-RX' + (' (simulato)' if simulato else ''))
        self.setWindowIcon(icona.icona())
        self.resize(900, 760)

        centro = QWidget()
        col = QVBoxLayout(centro)
        col.setContentsMargins(0, 8, 0, 0)
        self.intestazione = QLabel('Ricerca del ricevitore…')
        self.intestazione.setWordWrap(True)
        self.nuovo = QPushButton('Prepara un nuovo ricevitore…')
        self.nuovo.setToolTip('Installa PS-RX su un Raspberry Pi Pico 2 W nuovo o in modalità BOOTSEL')
        self.nuovo.clicked.connect(self.apri_procedura)
        self.nuovo.setVisible(False)
        testa = QHBoxLayout()
        testa.setContentsMargins(16, 0, 16, 0)
        testa.addWidget(self.intestazione, 1)
        testa.addWidget(self.nuovo)
        col.addLayout(testa)
        self.schede = QTabWidget()
        self.gamepad = SchedaGamepad(self)
        self.rete = SchedaRete(self)
        self.sistema = SchedaSistema(self)
        self.schede.addTab(self.gamepad, 'Gamepad')
        self.schede.addTab(self.rete, 'Rete')
        self.schede.addTab(self.sistema, 'Sistema')
        col.addWidget(self.schede)
        self.setCentralWidget(centro)
        self.statusBar()

        self.vassoio: Optional[QSystemTrayIcon] = None
        if QSystemTrayIcon.isSystemTrayAvailable():
            self.vassoio = QSystemTrayIcon(icona.icona(icona.GRIGIO), self)
            self.vassoio.setToolTip('PS-RX')
            menu = QMenu()
            menu.addAction('Apri PS-RX', self.mostra)
            self.azione_abbina = menu.addAction('Abbina un nuovo controller',
                                                lambda: self.esegui(lambda c: c.abbina(), 'Abbinamento aperto per 30 secondi'))
            menu.addSeparator()
            menu.addAction('Esci', self.esci)
            self.vassoio.setContextMenu(menu)
            self.vassoio.activated.connect(self._su_vassoio)
            self.vassoio.show()
        self._avvisato_vassoio = False
        self._uscita = False

        ponte.lav.istantanea.connect(self._su_istantanea)
        ponte.lav.notifiche.connect(self._su_notifiche)
        # Ricerca automatica degli aggiornamenti: dopo qualche secondo, cosi' la versione del firmware e' gia'
        # nota se il ricevitore e' collegato.
        QTimer.singleShot(8000, self._ricerca_all_avvio)
        # Pico nuovo o in BOOTSEL collegato senza un PS-RX: la procedura si propone da sola.
        self._timer_bootsel = QTimer(self)
        self._timer_bootsel.timeout.connect(self._cerca_bootsel)
        self._timer_bootsel.start(2000)

    # --- servizi per le schede --------------------------------------------------------------------
    def esegui(self, funzione: Callable, fatto_testo: str = '', fatto: Optional[Callable] = None) -> None:
        def ok(valore):
            if fatto_testo:
                self.messaggio(fatto_testo)
            if fatto:
                fatto(valore)

        self.ponte.esegui(funzione, ok, lambda e: self.messaggio(testo_errore(e), errore=True))

    def imposta(self, ident: int, valore: int) -> None:
        voce = schema.PER_ID[ident]
        if voce.get('riconnette'):
            collegati = self.ist.stato.pad_connessi if self.ist and self.ist.stato else 0
            domanda = 'Il ricevitore si ricollega all\'USB per cambiare forma'
            domanda += (f': per circa un secondo i {collegati} controller collegati non arrivano al PC. Continuare?'
                        if collegati else '. Continuare?')
            if not self.conferma(voce['titolo'], domanda):
                self._rimostra()
                return
        testo = f'{voce["titolo"]}: {schema.testo_valore(voce, valore)}'
        self.ponte.esegui(lambda c: c.imposta(ident, valore), lambda _: self.messaggio(testo),
                          lambda e: (self.messaggio(testo_errore(e), errore=True), self._rimostra()))

    def conferma(self, titolo: str, testo: str) -> bool:
        return QMessageBox.question(self, titolo, testo) == QMessageBox.Yes

    def messaggio(self, testo: str, errore: bool = False) -> None:
        self.statusBar().setStyleSheet('color: #c62828;' if errore else '')
        self.statusBar().showMessage(testo, 8000)

    def _rimostra(self) -> None:
        """Riporta i controlli al valore del ricevitore (dopo un annullamento o un errore)."""
        for s in (self.gamepad, self.rete, self.sistema):
            s.aggiorna(self.ist)

    # --- dati dal thread USB -------------------------------------------------------------------
    def _su_istantanea(self, ist: Optional[Istantanea]) -> None:
        self.ist = ist
        if ist is None or ist.stato is None:
            self.intestazione.setText('<b>Ricevitore non trovato.</b> Collega il PS-RX a una porta USB '
                                      '(se è aperto da un\'altra app, chiudila). Hai un Pico 2 W nuovo? Usa il '
                                      'pulsante qui accanto.')
            self.nuovo.setVisible(True)
            if self.vassoio:
                self.vassoio.setIcon(icona.icona(icona.GRIGIO))
                self.vassoio.setToolTip('PS-RX: non collegato')
        else:
            st = ist.stato
            n = st.pad_connessi
            testo = (f'<b>PS-RX collegato</b> · modalità {st.nome_modalita} · '
                     f'{n} controller {"collegato" if n == 1 else "collegati"}')
            if st.finestra_abbinamento:
                testo += ' · <b>abbinamento in corso</b>'
            self.intestazione.setText(testo)
            self.nuovo.setVisible(False)
            if self.vassoio:
                self.vassoio.setIcon(icona.icona(icona.BLU))
                self.vassoio.setToolTip(f'PS-RX: {n} controller, modalità {st.nome_modalita}')
        for s in (self.gamepad, self.rete, self.sistema):
            s.aggiorna(ist)

    def _su_notifiche(self, notifiche: list) -> None:
        if not self.vassoio or not opzioni.leggi('notifiche', True):
            return
        # Una notifica alla volta: se ne arrivano di piu' insieme (es. due controller accesi), si uniscono.
        if len(notifiche) == 1:
            titolo, testo, critica = notifiche[0]
        else:
            titolo = 'PS-RX'
            testo = '\n'.join(f'{t}: {x}' for t, x, _ in notifiche)
            critica = any(c for _, _, c in notifiche)
        icona_msg = QSystemTrayIcon.Warning if critica else QSystemTrayIcon.Information
        self.vassoio.showMessage(titolo, testo, icona_msg, 6000)

    # --- nuovo ricevitore ------------------------------------------------------------------------
    def apri_procedura(self) -> None:
        from .procedura import Procedura
        if self.procedura is not None and self.procedura.isVisible():
            self.procedura.raise_()
            return
        self.mostra()
        self.procedura = Procedura(self)
        self.procedura.show()

    def _cerca_bootsel(self) -> None:
        if self.ist is not None and self.ist.stato is not None:
            return
        if self.procedura is not None and self.procedura.isVisible():
            return
        try:
            presente = bool(pico_nuovo.trova())
        except OSError:
            presente = False
        if presente and not self._bootsel_visto:
            self._bootsel_visto = True
            if self.vassoio and not self.isVisible():
                self.vassoio.showMessage('PS-RX', 'Trovato un Raspberry Pi Pico pronto per il firmware.',
                                         QSystemTrayIcon.Information, 5000)
            self.apri_procedura()
        elif not presente:
            self._bootsel_visto = False

    # --- aggiornamenti ---------------------------------------------------------------------------
    def _ricerca_all_avvio(self) -> None:
        if self.aggiornatore.dovuta():
            versione = self.ist.info.versione if self.ist and self.ist.info else None
            self._ricerca_automatica = self.aggiornatore.cerca(versione)

    def _su_esito_aggiornamenti(self, c, errore: str) -> None:
        automatica, self._ricerca_automatica = self._ricerca_automatica, False
        if not automatica or c is None or not c.qualcosa or not self.vassoio:
            return
        cosa = ' e '.join(x for x, s in (('firmware', c.firmware_nuovo), ('app', c.app_nuova)) if s)
        self.vassoio.showMessage('PS-RX: aggiornamento disponibile',
                                 f'Versione {c.release.versione} ({cosa}). Apri l\'app → Sistema → Aggiornamenti.',
                                 QSystemTrayIcon.Information, 8000)

    def riavvia(self) -> None:
        """Aggiornamento dell'app avviato: libera l'istanza unica ed esce (l'installer riapre l'app)."""
        if self.server is not None:
            self.server.close()
        self.esci()

    # --- finestra e vassoio ---------------------------------------------------------------------
    def mostra(self) -> None:
        self.showNormal()
        self.raise_()
        self.activateWindow()

    def _su_vassoio(self, motivo) -> None:
        if motivo in (QSystemTrayIcon.Trigger, QSystemTrayIcon.DoubleClick):
            self.mostra()

    def showEvent(self, e) -> None:
        super().showEvent(e)
        self.ponte.lav.imposta_visibile(True)

    def hideEvent(self, e) -> None:
        super().hideEvent(e)
        self.ponte.lav.imposta_visibile(False)

    def closeEvent(self, e: QCloseEvent) -> None:
        if not self._uscita and self.vassoio and opzioni.leggi('resta_in_vassoio', True):
            e.ignore()
            self.hide()
            if not self._avvisato_vassoio:
                self._avvisato_vassoio = True
                self.vassoio.showMessage('PS-RX', 'L\'app resta qui per le notifiche. Per chiuderla: tasto destro '
                                         '→ Esci.', QSystemTrayIcon.Information, 4000)
            return
        e.accept()
        self.esci()

    def esci(self) -> None:
        self._uscita = True
        if self.vassoio:
            self.vassoio.hide()
        self.ponte.ferma()
        QApplication.quit()
