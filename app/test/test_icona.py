"""Icone dell'app: i PNG in psrx_app/risorse ci sono con le misure giuste e il .ico si crea con tutte le misure."""

import os
import struct
import sys
import tempfile
import unittest

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from PySide6.QtGui import QGuiApplication  # noqa: E402

from psrx_app import icona  # noqa: E402

APP = QGuiApplication.instance() or QGuiApplication([])


class TestIcona(unittest.TestCase):
    def test_file(self):
        self.assertEqual(sorted(s.width() for s in icona.icona().availableSizes()), list(icona.MISURE_APP))
        for collegato in (True, False):
            self.assertEqual(sorted(s.width() for s in icona.vassoio(collegato).availableSizes()), [16, 32])

    def test_ico(self):
        with tempfile.TemporaryDirectory() as cartella:
            percorso = os.path.join(cartella, 'ps-rx.ico')
            self.assertTrue(icona.salva_ico(percorso))
            with open(percorso, 'rb') as f:
                dati = f.read()
        n = struct.unpack('<HHH', dati[:6])[2]
        self.assertEqual([dati[6 + 16 * i] or 256 for i in range(n)], list(icona.MISURE_ICO))


if __name__ == '__main__':
    unittest.main()
