"""
PS-RX - lingua dei testi (italiano e inglese). Solo libreria standard.

I testi sono scritti in italiano e passano da t(): con la lingua italiana restano come sono, con qualsiasi
altra lingua diventano inglesi (dizionario in lingua_en.py). Un testo senza traduzione resta in italiano: un
test controlla che non ce ne siano.

    from psrx.lingua import t
    t('Ricevitore non trovato')
    t('Posto {n} spento', n=2)
"""

from __future__ import annotations

import locale
import os
import sys
from typing import Optional

from .lingua_en import EN

_lingua = 'it'


def normalizza(codice: Optional[str]) -> str:
    """'it', 'it_IT', 'Italian', 'italiano' -> 'it'; tutto il resto -> 'en'."""
    c = (codice or '').strip().lower()
    return 'it' if c.startswith('it') else 'en'


def imposta(codice: Optional[str]) -> str:
    global _lingua
    _lingua = normalizza(codice)
    return _lingua


def attuale() -> str:
    return _lingua


def di_sistema() -> str:
    """Lingua del sistema operativo (Windows: lingua dell'interfaccia; Linux: LANG/LC_*)."""
    if sys.platform == 'win32':
        try:
            import ctypes
            lcid = ctypes.windll.kernel32.GetUserDefaultUILanguage()
            return 'it' if (lcid & 0x3FF) == 0x10 else 'en'   # LANG_ITALIAN
        except (OSError, AttributeError):
            pass
    for variabile in ('LC_ALL', 'LC_MESSAGES', 'LANGUAGE', 'LANG'):
        if os.environ.get(variabile):
            return normalizza(os.environ[variabile])
    try:
        return normalizza(locale.getlocale()[0])
    except (ValueError, TypeError):
        return 'en'


def tr(testo: str, *valori, **campi) -> str:
    """Traduce e, se ci sono, inserisce i valori ({0}, {1}... oppure {nome})."""
    s = testo if _lingua == 'it' else EN.get(testo, testo)
    return s.format(*valori, **campi) if (valori or campi) else s


t = tr   # nome corto per i moduli che non usano gia' 't' come variabile


class Testi(dict):
    """Tabella di testi tradotti quando si leggono (non quando il modulo si carica, prima che la lingua sia
    scelta): TESTO_ERRORE[codice], .get(codice, predefinito), .items()."""

    def __getitem__(self, chiave):
        return tr(super().__getitem__(chiave))

    def get(self, chiave, predefinito=None):
        v = super().get(chiave)
        return predefinito if v is None else tr(v)

    def items(self):
        return [(k, tr(v)) for k, v in super().items()]

    def values(self):
        return [tr(v) for v in super().values()]
