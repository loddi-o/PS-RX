"""Italiano e inglese: ogni testo ha la traduzione, i segnaposto coincidono, la scelta della lingua."""

import os
import string
import sys
import unittest

QUI = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(QUI))
sys.path.insert(0, os.path.join(REPO, 'app'))
sys.path.insert(0, os.path.join(REPO, 'strumenti'))

from psrx import lingua, protocollo as p  # noqa: E402
from psrx.lingua_en import EN  # noqa: E402
import chiavi_lingua  # noqa: E402


def segnaposto(testo):
    return sorted(c[1] for c in string.Formatter().parse(testo) if c[1] is not None)


class TestLingua(unittest.TestCase):
    def tearDown(self):
        lingua.imposta('it')

    def test_ogni_testo_tradotto(self):
        mancanti = [c for c in chiavi_lingua.tutte() if c not in EN]
        self.assertEqual(mancanti, [], 'testi senza traduzione: strumenti/chiavi_lingua.py --mancanti')

    def test_segnaposto(self):
        for it, en in EN.items():
            self.assertEqual(segnaposto(it), segnaposto(en), it)

    def test_scelta(self):
        self.assertEqual(lingua.normalizza('it_IT'), 'it')
        self.assertEqual(lingua.normalizza('Italian'), 'it')
        self.assertEqual(lingua.normalizza('de_DE'), 'en')   # nessuna corrispondenza: inglese
        self.assertEqual(lingua.normalizza(''), 'en')
        lingua.imposta('en')
        self.assertEqual(lingua.tr('Posto {0}', 3), 'Slot 3')
        self.assertEqual(p.TESTO_ERRORE[p.ERR_COMANDO], 'unknown command')   # tabelle tradotte alla lettura
        self.assertEqual(p.TESTO_RETE.get(p.RETE_CONNESSA), 'connected')
        lingua.imposta('it')
        self.assertEqual(lingua.tr('Posto {0}', 3), 'Posto 3')
        self.assertEqual(p.TESTO_ERRORE[p.ERR_COMANDO], 'comando sconosciuto')

    def test_testo_mancante_resta_italiano(self):
        lingua.imposta('en')
        self.assertEqual(lingua.tr('testo che non esiste'), 'testo che non esiste')


if __name__ == '__main__':
    unittest.main()
