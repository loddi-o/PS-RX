#!/usr/bin/env python3
"""
PS-RX - testi da tradurre: tutte le chiamate tr('...') / t('...') della libreria e dell'app, i testi dello
schema e delle tabelle di protocollo.py (Testi). Li usa il test della lingua e, con --mancanti, stampa quelli
senza traduzione inglese.

    python strumenti/chiavi_lingua.py [--mancanti]
"""

import ast
import glob
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(REPO, 'app')
sys.path.insert(0, APP)


def da_chiamate(percorso: str) -> list:
    with open(percorso, encoding='utf-8') as f:
        albero = ast.parse(f.read())
    chiavi = []
    for n in ast.walk(albero):
        if isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id in ('tr', 't') and n.args:
            a = n.args[0]
            if isinstance(a, ast.Constant) and isinstance(a.value, str):
                chiavi.append(a.value)
        # tabelle Testi({...})
        if isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id == 'Testi' and n.args:
            d = n.args[0]
            if isinstance(d, ast.Dict):
                chiavi += [v.value for v in d.values if isinstance(v, ast.Constant) and isinstance(v.value, str)]
    return chiavi


def tutte() -> list:
    from psrx import schema
    chiavi = []
    for f in sorted(glob.glob(os.path.join(APP, 'psrx', '*.py')) + glob.glob(os.path.join(APP, 'psrx_app', '*.py')) + [os.path.join(APP, 'decky', 'main.py')]):
        if os.path.basename(f) in ('lingua.py', 'lingua_en.py'):
            continue
        chiavi += da_chiamate(f)
    for voce in schema.IMPOSTAZIONI + schema.IMPOSTAZIONI_PAD:
        chiavi.append(voce['titolo'])
        if voce.get('nota'):
            chiavi.append(voce['nota'])
        for _, testo in voce.get('opzioni', []):
            chiavi.append(testo)
    visti, uniche = set(), []
    for c in chiavi:
        if c not in visti:
            visti.add(c)
            uniche.append(c)
    return uniche


if __name__ == '__main__':
    from psrx.lingua_en import EN
    for c in tutte():
        if '--mancanti' not in sys.argv or c not in EN:
            print(json.dumps(c, ensure_ascii=False))
