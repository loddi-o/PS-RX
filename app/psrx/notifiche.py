"""
PS-RX - sorveglianza degli eventi per le notifiche (app Windows nell'area di notifica, plugin Decky).

Ogni giro fa UNA lettura "silenziosa" dello stato (il Pico non la conta come app aperta e non
interroga la radio); solo se ci sono eventi nuovi legge gli eventi e i nomi dei controller.
"""

from __future__ import annotations

from typing import List, Optional, Tuple

from . import protocollo as p

Notifica = Tuple[str, str, bool]   # (titolo, testo, critica)


class Sorvegliante:
    def __init__(self, client):
        self.client = client
        self.ultimo: Optional[int] = None

    def controlla(self) -> List[Notifica]:
        st = self.client.stato(silenzioso=True)
        if self.ultimo is None:
            # Avvio dell'app: niente notifiche per quello che e' successo prima.
            self.ultimo = st.ultimo_evento
            return []
        if st.ultimo_evento == self.ultimo:
            return []
        if (st.ultimo_evento - self.ultimo) & 0xFFFF > 0x8000:
            self.ultimo = 0   # il ricevitore si e' riavviato: la numerazione riparte
        eventi = self.client.eventi(self.ultimo)
        if not eventi:
            self.ultimo = st.ultimo_evento
            return []
        self.ultimo = eventi[-1].numero
        nomi = {}
        try:
            nomi = {a.mac: a.nome for a in self.client.abbinati() if a.nome}
        except Exception:  # noqa: BLE001 - i nomi sono un di piu'
            pass
        return [(*p.testo_evento(e, nomi.get(e.mac, '')), e.tipo == p.EVENTO_BATTERIA and e.critico) for e in eventi]
