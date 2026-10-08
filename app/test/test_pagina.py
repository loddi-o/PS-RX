"""
La pagina WebUSB (web/ps-rx.html) ha una copia dello schema e dei numeri del protocollo: devono restare
uguali alla libreria. Se lo schema cambia: python strumenti/genera_pagina.py
"""

import json
import os
import re
import sys
import unittest
from html.parser import HTMLParser

QUI = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(QUI))
sys.path.insert(0, os.path.join(REPO, 'app'))
sys.path.insert(0, os.path.join(REPO, 'strumenti'))

from psrx import protocollo as p  # noqa: E402
import chiavi_ts  # noqa: E402
import genera_pagina  # noqa: E402


class TestiFissi(HTMLParser):
    """Testi e segnaposto dell'HTML prima dello script: traduciPagina() li cerca in EN."""
    def __init__(self):
        super().__init__()
        self.testi = []

    def handle_data(self, dati):
        testo = re.sub(r'\s+', ' ', dati).strip()
        # Uguali nelle due lingue, compresi i nomi delle lingue del selettore.
        uguali = ('PS-RX', 'Wake-on-LAN', 'WPA3', 'Password', 'Firmware', 'English', 'Italiano')
        if re.search(r'[a-zà-ù]{3}', testo) and testo not in uguali:
            self.testi.append(testo)

    def handle_starttag(self, tag, attributi):
        self.testi += [v for k, v in attributi if k == 'placeholder' and re.search(r'[a-z]{3}', v or '')]


class TestPagina(unittest.TestCase):
    def setUp(self):
        with open(genera_pagina.PAGINA, encoding='utf-8') as f:
            self.testo = f.read()

    def test_schema_aggiornato(self):
        self.assertEqual(genera_pagina.pagina_aggiornata(self.testo), self.testo,
                         'schema della pagina vecchio: lancia python strumenti/genera_pagina.py')

    def test_inglese(self):
        en = chiavi_ts.dizionario(genera_pagina.PAGINA)
        mancanti = [k for k in chiavi_ts.chiavi(genera_pagina.PAGINA) if k not in en]
        fissi = TestiFissi()
        fissi.feed(self.testo[self.testo.index('<body>'):self.testo.index('<script>')])
        mancanti += [k for k in fissi.testi if k not in en]
        self.assertEqual(mancanti, [], 'testi della pagina senza traduzione in EN')
        for it, testo_en in en.items():
            self.assertEqual(re.findall(r'\{\d+\}', it), re.findall(r'\{\d+\}', testo_en), it)

    def test_comandi(self):
        corpo = re.search(r'const CMD = \{(.*?)\};', self.testo, re.S).group(1)
        comandi = {k: int(v, 16) for k, v in re.findall(r'(\w+): (0x[0-9A-Fa-f]+)', corpo)}
        self.assertGreater(len(comandi), 20)
        for nome, valore in comandi.items():
            self.assertEqual(getattr(p, 'CMD_' + nome), valore, nome)

    def test_errori(self):
        corpo = re.search(r'const ERRORI = \[(.*?)\];', self.testo, re.S).group(1)
        self.assertEqual(len(re.findall(r'"[^"]*"', corpo)), len(p.TESTO_ERRORE))

    def test_lunghezze(self):
        self.assertIn(f'0: {p.DIM_INFO}, 1: {p.DIM_STATO}', self.testo)
        self.assertIn('RETI_MAX * 35 + 13', self.testo)
        self.assertEqual(p.DIM_RETI, p.RETI_MAX * 35 + 13)


if __name__ == '__main__':
    unittest.main()
