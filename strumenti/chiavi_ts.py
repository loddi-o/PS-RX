"""
Testi passati a t("...") in un file TypeScript/JavaScript (plugin Decky, pagina WebUSB).
Uso: python strumenti/chiavi_ts.py file [--mancanti dizionario.ts]
"""
import json
import re
import sys

CHIAMATA = re.compile(r'\bt\(("(?:[^"\\\n]|\\.)*")')


def chiavi(percorso: str) -> list:
    with open(percorso, encoding='utf-8') as f:
        testo = f.read()
    viste = []
    for m in CHIAMATA.finditer(testo):
        k = json.loads(m.group(1))
        if k not in viste:
            viste.append(k)
    return viste


def dizionario(percorso: str) -> dict:
    """Chiavi di un dizionario scritto una voce per riga: "italiano": "english","""
    with open(percorso, encoding='utf-8') as f:
        testo = f.read()
    voci = {}
    for riga in testo.splitlines():
        m = re.match(r'^\s*("(?:[^"\\]|\\.)*")\s*:\s*("(?:[^"\\]|\\.)*"),?\s*$', riga)
        if m:
            voci[json.loads(m.group(1))] = json.loads(m.group(2))
    return voci


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    elenco = chiavi(sys.argv[1])
    if '--mancanti' in sys.argv:
        d = dizionario(sys.argv[sys.argv.index('--mancanti') + 1])
        elenco = [k for k in elenco if k not in d]
    for k in elenco:
        print(json.dumps(k, ensure_ascii=False))
