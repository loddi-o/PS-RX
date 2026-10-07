"""
PS-RX - Linux: permesso di parlare col ricevitore via usbfs senza essere root.

Il nodo /dev/bus/usb/BBB/DDD di solito e' scrivibile solo da root. La regola udev qui sotto lo rende
scrivibile per le identita' USB che il ricevitore usa (DualSense, Xbox 360, Steam Controller): le
richieste che l'app manda sono solo quelle vendor di PS-RX, che un controller vero rifiuta. Il plugin
Decky gira come root e la installa da solo; la CLI la stampa (python -m psrx regola-udev).
Su Windows non serve: WinUSB si aggancia da solo all'interfaccia di configurazione.
"""

from __future__ import annotations

import os
import subprocess
import tempfile
from typing import Tuple

from . import protocollo as p

PERCORSO = '/etc/udev/rules.d/70-ps-rx.rules'


def _regola() -> str:
    righe = ['# PS-RX: app di gestione del ricevitore (richieste di controllo USB via usbfs).',
             '# Il ricevitore si presenta come DualSense, Xbox 360 o Steam Controller secondo la modalita\'.']
    for vid, pid in p.IDENTITA:
        righe.append(f'SUBSYSTEM=="usb", ENV{{DEVTYPE}}=="usb_device", ATTR{{idVendor}}=="{vid:04x}", '
                     f'ATTR{{idProduct}}=="{pid:04x}", MODE="0666"')
    righe.append('# WebUSB (pagina di configurazione in Chrome/Edge) usa gli stessi nodi.')
    return '\n'.join(righe) + '\n'


REGOLA = _regola()
_RICARICA = ['udevadm control --reload-rules', 'udevadm trigger --subsystem-match=usb']


def regola_installata() -> bool:
    try:
        with open(PERCORSO) as f:
            return f.read() == REGOLA
    except OSError:
        return False


def installa_come_root() -> Tuple[bool, str]:
    """Per il plugin Decky (gira come root)."""
    try:
        with open(PERCORSO, 'w') as f:
            f.write(REGOLA)
        for comando in _RICARICA:
            subprocess.run(comando.split(), check=False, timeout=10)
        return True, f'regola udev installata in {PERCORSO}'
    except OSError as e:
        return False, f'regola udev non installata: {e}'


def installa_con_pkexec() -> Tuple[bool, str]:
    """Per un'app desktop su Linux: chiede la password con la finestra di sistema."""
    with tempfile.NamedTemporaryFile('w', suffix='.rules', delete=False) as f:
        f.write(REGOLA)
        temporaneo = f.name
    try:
        script = f'install -m 0644 {temporaneo} {PERCORSO} && ' + ' && '.join(_RICARICA)
        r = subprocess.run(['pkexec', 'sh', '-c', script], capture_output=True, text=True, timeout=120)
        return r.returncode == 0, r.stderr.strip() or 'regola udev installata'
    except (OSError, subprocess.TimeoutExpired) as e:
        return False, str(e)
    finally:
        os.unlink(temporaneo)
