"""Aggiornamenti da GitHub: versioni, lettura delle release, download con controllo dell'impronta."""

import hashlib
import os
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from psrx import aggiornamenti as ag  # noqa: E402

RISPOSTA = {
    'tag_name': 'v0.2.0', 'name': 'PS-RX 0.2.0', 'body': 'note', 'html_url': 'https://github.com/loddi-o/PS-RX/releases/tag/v0.2.0',
    'assets': [
        {'name': 'ps-rx-firmware-0.2.0.uf2', 'browser_download_url': 'https://x/fw.uf2', 'size': 10,
         'digest': 'sha256:' + 'a' * 64},
        {'name': 'PS-RX-Setup-0.2.0.exe', 'browser_download_url': 'https://x/app.exe', 'size': 20},
        {'name': 'PS-RX-0.2.0.exe', 'browser_download_url': 'https://x/portatile.exe', 'size': 20},
        {'name': 'ps-rx-decky-0.2.0.zip', 'browser_download_url': 'https://x/d.zip', 'size': 30},
        {'name': 'ps-rx-0.2.0.html', 'browser_download_url': 'https://x/p.html', 'size': 40},
        {'name': 'altro.txt', 'browser_download_url': 'https://x/altro', 'size': 1},
    ],
}


class TestVersioni(unittest.TestCase):
    def test_numero(self):
        self.assertEqual(ag.numero('ps-rx-0.2.1'), (0, 2, 1))
        self.assertEqual(ag.numero('v1.10'), (1, 10, 0))
        self.assertIsNone(ag.numero('ps-rx-dev'))

    def test_piu_nuova(self):
        self.assertTrue(ag.piu_nuova('0.2.0', 'ps-rx-0.1.9'))
        self.assertFalse(ag.piu_nuova('0.2.0', 'ps-rx-0.2.0'))
        self.assertFalse(ag.piu_nuova('0.2.0', '0.10.0'))
        self.assertTrue(ag.piu_nuova('0.2.0', 'ps-rx-dev'))     # build di sviluppo: sempre piu' vecchia


class TestRelease(unittest.TestCase):
    def test_da_json(self):
        r = ag.da_json(RISPOSTA)
        self.assertEqual(r.versione, '0.2.0')
        self.assertEqual(set(r.file), {'firmware', 'app_windows', 'decky', 'pagina'})
        self.assertEqual(r.file['firmware'].sha256, 'a' * 64)
        self.assertIsNone(r.file['app_windows'].sha256)
        self.assertEqual(r.file['app_windows'].nome, 'PS-RX-Setup-0.2.0.exe')   # l'installer, non l'exe sciolto

    def test_controlla(self):
        vero = ag.ultima_release
        ag.ultima_release = lambda: ag.da_json(RISPOSTA)
        try:
            c = ag.controlla('ps-rx-0.1.0', '0.1.0')
            self.assertTrue(c.firmware_nuovo and c.app_nuova and c.qualcosa)
            c = ag.controlla(None, '0.2.0')                     # ricevitore non collegato
            self.assertFalse(c.firmware_nuovo or c.app_nuova)
            c = ag.controlla('ps-rx-dev', '0.2.0', tipo_app='decky')
            self.assertTrue(c.firmware_nuovo)
            self.assertFalse(c.app_nuova)
        finally:
            ag.ultima_release = vero


class TestScarica(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix='psrx-agg-')
        self.sorgente = os.path.join(self.dir, 'sorgente.bin')
        self.dati = os.urandom(200_000)
        with open(self.sorgente, 'wb') as f:
            f.write(self.dati)
        self.url = pathlib.Path(self.sorgente).as_uri()

    def test_impronta_giusta(self):
        f = ag.File('ps-rx-firmware-0.2.0.uf2', self.url, len(self.dati), hashlib.sha256(self.dati).hexdigest())
        visti = []
        percorso = ag.scarica(f, os.path.join(self.dir, 'out'), lambda a, b: visti.append(a))
        with open(percorso, 'rb') as letto:
            self.assertEqual(letto.read(), self.dati)
        self.assertEqual(visti[-1], len(self.dati))

    def test_impronta_sbagliata(self):
        f = ag.File('x.uf2', self.url, len(self.dati), '0' * 64)
        with self.assertRaises(ag.ErroreAggiornamento):
            ag.scarica(f, os.path.join(self.dir, 'out'))
        self.assertFalse(os.path.exists(os.path.join(self.dir, 'out', 'x.uf2')))

    def test_dimensione_sbagliata(self):
        f = ag.File('y.uf2', self.url, len(self.dati) + 1, None)
        with self.assertRaises(ag.ErroreAggiornamento):
            ag.scarica(f, os.path.join(self.dir, 'out'))

    def test_annullato(self):
        f = ag.File('z.uf2', self.url, len(self.dati), None)
        with self.assertRaises(ag.ErroreAggiornamento):
            ag.scarica(f, os.path.join(self.dir, 'out'), interrotto=lambda: True)


if __name__ == '__main__':
    unittest.main()
