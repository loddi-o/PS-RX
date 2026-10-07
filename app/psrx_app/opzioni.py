"""
PS-RX app - preferenze dell'app (QSettings) e avvio con Windows (chiave Run dell'utente, niente privilegi).
"""

from __future__ import annotations

import os
import sys

from PySide6.QtCore import QSettings

AVVIO_DISPONIBILE = sys.platform == 'win32'
_CHIAVE_RUN = r'Software\Microsoft\Windows\CurrentVersion\Run'
_NOME_RUN = 'PS-RX'


def _impostazioni() -> QSettings:
    return QSettings('PS-RX', 'PS-RX')


def leggi(chiave: str, predefinito):
    v = _impostazioni().value(chiave, predefinito)
    if isinstance(predefinito, bool) and isinstance(v, str):
        return v.lower() in ('true', '1')
    return v


def scrivi(chiave: str, valore) -> None:
    _impostazioni().setValue(chiave, valore)


def comando_avvio() -> str:
    """Riga di comando per l'avvio con Windows: l'exe, oppure pythonw con lo script di avvio (sviluppo)."""
    if getattr(sys, 'frozen', False):
        return f'"{sys.executable}" --avvio'
    pythonw = os.path.join(os.path.dirname(sys.executable), 'pythonw.exe')
    script = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'ps-rx.pyw')
    return f'"{pythonw}" "{script}" --avvio'


def avvio_automatico() -> bool:
    if not AVVIO_DISPONIBILE:
        return False
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, _CHIAVE_RUN) as k:
            winreg.QueryValueEx(k, _NOME_RUN)
            return True
    except OSError:
        return False


def imposta_avvio_automatico(attivo: bool) -> None:
    if not AVVIO_DISPONIBILE:
        return
    import winreg
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, _CHIAVE_RUN, 0, winreg.KEY_SET_VALUE) as k:
        if attivo:
            winreg.SetValueEx(k, _NOME_RUN, 0, winreg.REG_SZ, comando_avvio())
        else:
            try:
                winreg.DeleteValue(k, _NOME_RUN)
            except OSError:
                pass
