"""
PS-RX app - thread di lavoro: tutto l'I/O USB passa da qui, la finestra non aspetta mai il ricevitore.

Ogni giro (1 s):
  - finestra aperta: stato completo (una lettura "normale": il Pico sa che un'app guarda e aggiorna il
    segnale dei controller) e, ogni 5 giri o dopo una modifica, impostazioni, abbinati e reti;
  - finestra chiusa (solo area di notifica): ogni 2 s una lettura "silenziosa" per le notifiche, poi il
    dispositivo si chiude, cosi' la pagina WebUSB o la riga di comando lo possono aprire.
Le richieste dell'interfaccia (impostazioni, abbinamento, caricamento del firmware...) passano dalla
stessa coda, in ordine.

Per il ricevitore sono richieste di controllo sull'endpoint 0: il PC le programma dopo i report dei
controller (trasferimenti periodici) e il Pico le serve in pochi microsecondi nel ciclo principale.
"""

from __future__ import annotations

import time
import traceback
from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional

from PySide6.QtCore import QMetaObject, QObject, QThread, QTimer, Qt, Signal, Slot

from psrx import protocollo as p
from psrx.errori import ErrorePsrx, PermessoNegato, Scollegato
from psrx.notifiche import Sorvegliante
from psrx.servizio import Psrx

GIRO_MS = 1000
GIRI_DETTAGLI = 5


@dataclass
class Istantanea:
    info: Optional[p.Info] = None
    stato: Optional[p.Stato] = None
    impostazioni: Dict[int, int] = field(default_factory=dict)
    abbinati: List[p.Abbinato] = field(default_factory=list)
    reti: Optional[p.Reti] = None
    t: float = 0.0


def testo_errore(e: BaseException) -> str:
    if isinstance(e, ErrorePsrx):
        if e.codice == p.ERR_COMANDO:   # funzione nuova dell'app, firmware vecchio
            return ('il firmware del ricevitore è troppo vecchio per questa funzione: aggiornalo da Sistema → '
                    'Aggiornamenti')
        return str(e)
    if isinstance(e, Scollegato):
        return 'Ricevitore non trovato (scollegato, oppure aperto da un\'altra app)'
    if isinstance(e, PermessoNegato):
        return 'Permesso negato sul dispositivo USB'
    return f'{type(e).__name__}: {e}'


class Lavoratore(QObject):
    # verso la finestra (connessioni in coda: arrivano nel thread dell'interfaccia)
    istantanea = Signal(object)        # Istantanea, oppure None se il ricevitore non c'e'
    notifiche = Signal(list)           # [(titolo, testo, critica)]
    risultato = Signal(int, object, object)   # (numero richiesta, valore, eccezione)
    # dalla finestra
    _esegui = Signal(int, object)
    _visibile = Signal(bool)
    _aggiorna = Signal()

    def __init__(self, apri: Optional[Callable] = None):
        super().__init__()
        self._apri = apri
        self.client: Optional[Psrx] = None
        self.sorvegliante: Optional[Sorvegliante] = None
        self.visibile = False
        self.giro = 0
        self.dettagli_dovuti = True
        self.ultima = Istantanea()
        self.timer: Optional[QTimer] = None
        self._esegui.connect(self._su_esegui)
        self._visibile.connect(self._su_visibile)
        self._aggiorna.connect(self._su_aggiorna)

    # --- dal thread dell'interfaccia --------------------------------------------------------
    def esegui(self, numero: int, funzione: Callable) -> None:
        self._esegui.emit(numero, funzione)

    def imposta_visibile(self, visibile: bool) -> None:
        self._visibile.emit(visibile)

    def aggiorna_subito(self) -> None:
        self._aggiorna.emit()

    # --- nel thread di lavoro -----------------------------------------------------------------
    @Slot()
    def avvia(self) -> None:
        self.client = Psrx(self._apri) if self._apri else Psrx()
        self.sorvegliante = Sorvegliante(self.client)
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._su_giro)
        self.timer.start(GIRO_MS)
        self._su_giro()

    @Slot()
    def ferma(self) -> None:
        if self.timer:
            self.timer.stop()
        if self.client:
            self.client.chiudi()

    @Slot(bool)
    def _su_visibile(self, visibile: bool) -> None:
        self.visibile = visibile
        if visibile:
            self.dettagli_dovuti = True
            self._su_giro()

    @Slot()
    def _su_aggiorna(self) -> None:
        self.dettagli_dovuti = True
        self._su_giro()

    @Slot(int, object)
    def _su_esegui(self, numero: int, funzione: Callable) -> None:
        try:
            valore = funzione(self.client)
            self.risultato.emit(numero, valore, None)
        except Exception as e:  # noqa: BLE001 - l'errore va alla finestra
            if not isinstance(e, (ErrorePsrx, Scollegato, PermessoNegato)):
                traceback.print_exc()
            self.risultato.emit(numero, None, e)
        self.dettagli_dovuti = True
        if self.visibile:
            self._su_giro()

    @Slot()
    def _su_giro(self) -> None:
        self.giro += 1
        try:
            if self.visibile:
                self._giro_completo()
            elif self.giro % 2 == 0:
                self._giro_notifiche()
                self.client.chiudi()
        except (Scollegato, PermessoNegato):
            self.client.chiudi()
            self.ultima = Istantanea()
            self.dettagli_dovuti = True
            self.istantanea.emit(None)
        except Exception:  # noqa: BLE001 - un giro andato male non ferma l'app
            traceback.print_exc()
            self.client.chiudi()

    def _giro_completo(self) -> None:
        c = self.client
        s = self.ultima
        if s.info is None:
            s.info = c.info()
            self.dettagli_dovuti = True
        stato = c.stato()
        if s.stato is not None and stato.uptime_s < s.stato.uptime_s:
            self.dettagli_dovuti = True    # il ricevitore si e' riavviato: rileggo tutto, versione compresa
        s.stato = stato
        if self.dettagli_dovuti or self.giro % GIRI_DETTAGLI == 0:
            # Anche la versione: dopo un aggiornamento il Pico si riavvia e il client si ricollega da solo,
            # senza passare da "scollegato".
            s.info = c.info()
            s.impostazioni = c.impostazioni()
            s.abbinati = c.abbinati()
            s.reti = c.reti()
            self.dettagli_dovuti = False
        s.t = time.monotonic()
        self.istantanea.emit(Istantanea(s.info, s.stato, dict(s.impostazioni), list(s.abbinati), s.reti, s.t))
        self._giro_notifiche()

    def _giro_notifiche(self) -> None:
        nuove = self.sorvegliante.controlla()
        if nuove:
            self.notifiche.emit(nuove)


class Ponte(QObject):
    """Lato interfaccia: avvia il thread e abbina ogni richiesta alla sua risposta."""

    def __init__(self, apri: Optional[Callable] = None):
        super().__init__()
        self.thread = QThread()
        self.thread.setObjectName('ps-rx-usb')
        self.lav = Lavoratore(apri)
        self.lav.moveToThread(self.thread)
        self.thread.started.connect(self.lav.avvia)
        self.lav.risultato.connect(self._su_risultato)
        self._attese: Dict[int, tuple] = {}
        self._numero = 0

    def avvia(self) -> None:
        self.thread.start()

    def ferma(self) -> None:
        if self.thread.isRunning():
            QMetaObject.invokeMethod(self.lav, 'ferma', Qt.BlockingQueuedConnection)
        self.thread.quit()
        self.thread.wait(3000)

    def esegui(self, funzione: Callable, fatto: Optional[Callable] = None, errore: Optional[Callable] = None) -> None:
        self._numero += 1
        self._attese[self._numero] = (fatto, errore)
        self.lav.esegui(self._numero, funzione)

    @Slot(int, object, object)
    def _su_risultato(self, numero: int, valore, eccezione) -> None:
        fatto, errore = self._attese.pop(numero, (None, None))
        if eccezione is None:
            if fatto:
                fatto(valore)
        elif errore:
            errore(eccezione)
