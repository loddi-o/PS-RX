"""Firmware su un Pico nuovo: riconoscimento della chiavetta del bootloader e copia del file .uf2."""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from psrx import pico_nuovo  # noqa: E402

INFO_RP2350 = 'UF2 Bootloader v1.0\nModel: Raspberry Pi RP2350\nBoard-ID: RP2350\n'


class TestPicoNuovo(unittest.TestCase):
    def setUp(self):
        self.chiavetta = tempfile.mkdtemp(prefix='psrx-rp2350-')
        with open(os.path.join(self.chiavetta, pico_nuovo.INFO), 'w') as f:
            f.write(INFO_RP2350)

    def test_riconosce_rp2350(self):
        self.assertTrue(pico_nuovo._e_rp2350(self.chiavetta))
        altra = tempfile.mkdtemp()
        with open(os.path.join(altra, pico_nuovo.INFO), 'w') as f:
            f.write('Model: Raspberry Pi RP2\nBoard-ID: RPI-RP2\n')   # Pico 1 (RP2040): non va
        self.assertFalse(pico_nuovo._e_rp2350(altra))
        self.assertFalse(pico_nuovo._e_rp2350(tempfile.mkdtemp()))

    def test_scrivi_uf2(self):
        sorgente = os.path.join(tempfile.mkdtemp(), 'fw.uf2')
        dati = os.urandom(512 * 300)
        with open(sorgente, 'wb') as f:
            f.write(dati)
        visti = []
        pico_nuovo.scrivi_uf2(self.chiavetta, sorgente, lambda fatti, totali: visti.append((fatti, totali)))
        with open(os.path.join(self.chiavetta, pico_nuovo.NOME_FILE), 'rb') as f:
            self.assertEqual(f.read(), dati)
        self.assertEqual(visti[-1], (len(dati), len(dati)))

    def test_attendi_psrx(self):
        from psrx.errori import Scollegato
        tentativi = []

        def apri():
            tentativi.append(1)
            if len(tentativi) < 3:
                raise Scollegato('non ancora')
            return 'trasporto'

        vero = pico_nuovo.time.sleep
        pico_nuovo.time.sleep = lambda s: None
        try:
            self.assertEqual(pico_nuovo.attendi_psrx(apri, timeout_s=5), 'trasporto')
            with self.assertRaises(TimeoutError):
                pico_nuovo.attendi_psrx(lambda: (_ for _ in ()).throw(Scollegato('mai')), timeout_s=0.01)
        finally:
            pico_nuovo.time.sleep = vero


if __name__ == '__main__':
    unittest.main()
