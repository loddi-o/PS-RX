#!/usr/bin/env python3
"""
PS-RX - inserisce lo schema delle impostazioni (app/psrx/schema.py), con la traduzione inglese dei suoi testi,
nella pagina WebUSB web/ps-rx.html fra i segni SCHEMA-INIZIO e SCHEMA-FINE. Da rilanciare dopo ogni modifica dello schema (un test lo controlla).

    python strumenti/genera_pagina.py
"""

import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'app'))

from psrx import schema  # noqa: E402
from psrx.lingua_en import EN  # noqa: E402

PAGINA = os.path.join(REPO, 'web', 'ps-rx.html')
SEGNI = re.compile(r'(// SCHEMA-INIZIO\n)(.*?)(// SCHEMA-FINE)', re.S)


def riga_schema() -> str:
    dati = {'impostazioni': schema.IMPOSTAZIONI, 'impostazioni_pad': schema.IMPOSTAZIONI_PAD}
    testi = []
    for voce in schema.IMPOSTAZIONI + schema.IMPOSTAZIONI_PAD:
        testi += [voce['titolo'], voce.get('nota', '')] + [testo for _, testo in voce.get('opzioni', [])]
    inglese = {k: EN[k] for k in testi if k in EN}
    return ('const SCHEMA = ' + json.dumps(dati, ensure_ascii=False, separators=(',', ':')) + ';\n' +
            'const SCHEMA_EN = ' + json.dumps(inglese, ensure_ascii=False, separators=(',', ':')) + ';\n')


def pagina_aggiornata(testo: str) -> str:
    nuovo, n = SEGNI.subn(lambda m: m.group(1) + riga_schema() + m.group(3), testo)
    if n != 1:
        raise SystemExit('segni SCHEMA-INIZIO/SCHEMA-FINE non trovati nella pagina')
    return nuovo


def main() -> int:
    with open(PAGINA, encoding='utf-8') as f:
        testo = f.read()
    nuovo = pagina_aggiornata(testo)
    if nuovo != testo:
        with open(PAGINA, 'w', encoding='utf-8', newline='\n') as f:
            f.write(nuovo)
        print('[PS-RX] schema aggiornato in web/ps-rx.html')
    else:
        print('[PS-RX] web/ps-rx.html gia\' aggiornata')
    return 0


if __name__ == '__main__':
    sys.exit(main())
