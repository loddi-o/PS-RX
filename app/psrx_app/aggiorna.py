"""
PS-RX app - ricerca e download degli aggiornamenti da GitHub, in un thread a parte (mai in quello USB,
cosi' la rete non rallenta il ricevitore), e sostituzione dell'exe dell'app.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import threading
import time
from typing import Optional

from PySide6.QtCore import QObject, Signal

from psrx import aggiornamenti as ag

from . import opzioni

CARTELLA = os.path.join(tempfile.gettempdir(), 'ps-rx-aggiornamenti')
INTERVALLO_AUTOMATICO_S = 24 * 3600


def exe_sostituibile() -> bool:
    """L'app si aggiorna da sola solo come exe (PyInstaller); da sorgente mostra il link."""
    return bool(getattr(sys, 'frozen', False)) and sys.platform == 'win32'


def pulisci_vecchio_exe() -> None:
    """All'avvio: toglie l'exe precedente lasciato dall'ultimo aggiornamento."""
    if exe_sostituibile():
        try:
            os.remove(sys.executable + '.vecchio')
        except OSError:
            pass


def sostituisci_exe(nuovo: str) -> str:
    """Mette il nuovo exe al posto di quello in esecuzione (Windows permette di rinominare un exe aperto,
    non di cancellarlo) e restituisce il percorso da avviare."""
    corrente = sys.executable
    vecchio = corrente + '.vecchio'
    try:
        os.remove(vecchio)
    except OSError:
        pass
    os.replace(corrente, vecchio)
    try:
        os.replace(nuovo, corrente)
    except OSError:
        os.replace(vecchio, corrente)   # rimetto a posto quello di prima
        raise
    return corrente


def avvia_nuovo(percorso: str) -> None:
    subprocess.Popen([percorso], close_fds=True, creationflags=getattr(subprocess, 'DETACHED_PROCESS', 0))


class Aggiornatore(QObject):
    esito = Signal(object, str)          # (ag.Controllo o None, errore)
    avanzamento = Signal(int, int)       # byte scaricati, totali
    scaricato = Signal(str, str, str)    # (tipo, percorso, errore)

    def __init__(self):
        super().__init__()
        self.ultimo: Optional[ag.Controllo] = None
        self._occupato = False
        self.interrompi = False

    def cerca(self, firmware_installato: Optional[str]) -> bool:
        if self._occupato:
            return False
        self._occupato = True

        def lavoro():
            try:
                c = ag.controlla(firmware_installato)
                self.ultimo = c
                opzioni.scrivi('ultima_ricerca', int(time.time()))
                self.esito.emit(c, '')
            except Exception as e:  # noqa: BLE001 - l'errore va alla finestra
                self.esito.emit(None, str(e))
            finally:
                self._occupato = False

        threading.Thread(target=lavoro, name='ps-rx-aggiornamenti', daemon=True).start()
        return True

    def dovuta(self) -> bool:
        """Ricerca automatica: attiva e non fatta nelle ultime 24 ore."""
        if not opzioni.leggi('cerca_aggiornamenti', True):
            return False
        try:
            ultima = int(opzioni.leggi('ultima_ricerca', 0))
        except (TypeError, ValueError):
            ultima = 0
        return time.time() - ultima > INTERVALLO_AUTOMATICO_S

    def scarica(self, tipo: str, f: ag.File) -> None:
        self.interrompi = False

        def lavoro():
            try:
                percorso = ag.scarica(f, CARTELLA, lambda fatti, totali: self.avanzamento.emit(fatti, totali),
                                      lambda: self.interrompi)
                self.scaricato.emit(tipo, percorso, '')
            except Exception as e:  # noqa: BLE001
                self.scaricato.emit(tipo, '', str(e))

        threading.Thread(target=lavoro, name='ps-rx-download', daemon=True).start()
