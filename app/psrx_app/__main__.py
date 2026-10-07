"""
PS-RX app per Windows (funziona anche su Linux per prove).

    python -m psrx_app               finestra + icona nell'area di notifica
    python -m psrx_app --avvio       solo icona (avvio con Windows)
    python -m psrx_app --simulatore  ricevitore finto, per provare senza hardware

Una sola istanza: se l'app e' gia' aperta, la nuova chiede alla vecchia di mostrare la finestra ed esce.
"""

from __future__ import annotations

import argparse
import sys

from PySide6.QtCore import QTimer
from PySide6.QtNetwork import QLocalServer, QLocalSocket
from PySide6.QtWidgets import QApplication

from . import VERSIONE
from .finestra import Finestra
from .lavoratore import Ponte

NOME_ISTANZA = 'ps-rx-app'


def _gia_aperta() -> bool:
    s = QLocalSocket()
    s.connectToServer(NOME_ISTANZA)
    if s.waitForConnected(300):
        s.write(b'mostra')
        s.waitForBytesWritten(300)
        s.disconnectFromServer()
        return True
    return False


def main(argv=None) -> int:
    a = argparse.ArgumentParser(prog='ps-rx', description='PS-RX: impostazioni e notifiche del ricevitore')
    a.add_argument('--avvio', action='store_true', help='parti solo nell\'area di notifica')
    a.add_argument('--simulatore', action='store_true', help='usa un ricevitore simulato')
    args = a.parse_args(argv)

    app = QApplication(sys.argv[:1])
    app.setApplicationName('PS-RX')
    app.setApplicationVersion(VERSIONE)
    app.setQuitOnLastWindowClosed(False)

    if not args.simulatore and _gia_aperta():
        return 0
    server = QLocalServer()
    QLocalServer.removeServer(NOME_ISTANZA)
    server.listen(NOME_ISTANZA)

    apri = None
    pico = None
    if args.simulatore:
        from psrx.simulatore import PicoSimulato
        from psrx import protocollo as p
        pico = PicoSimulato()
        apri = lambda: pico.trasporto  # noqa: E731

    ponte = Ponte(apri)
    finestra = Finestra(ponte, simulato=args.simulatore)

    def nuova_connessione():
        c = server.nextPendingConnection()
        if c is not None:
            c.readyRead.connect(lambda: (c.readAll(), finestra.mostra()))
    server.newConnection.connect(nuova_connessione)

    ponte.avvia()
    if not args.avvio or finestra.vassoio is None:
        finestra.mostra()

    if pico is not None:
        # Simulazione: un secondo controller si collega dopo 4 s (per vedere notifiche e posti).
        def collega():
            pico.connetti_pad(1, p.mac_da_testo('A0:AB:51:00:11:22'), p.MODELLO_DUALSHOCK4, 15)
            pico.evento(p.EVENTO_COLLEGATO, 1)
            pico.evento(p.EVENTO_BATTERIA, 1)
        QTimer.singleShot(4000, collega)

    return app.exec()


if __name__ == '__main__':
    sys.exit(main())
