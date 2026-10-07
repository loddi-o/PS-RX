"""
Prova del backend del plugin Decky (app/decky/main.py) senza Decky: modulo "decky" finto e ricevitore
simulato al posto dell'USB. Controlla che i metodi chiamati dal frontend rispondano come previsto.
"""

import asyncio
import importlib.util
import os
import sys
import tempfile
import types
import unittest

QUI = os.path.dirname(os.path.abspath(__file__))
APP = os.path.dirname(QUI)
sys.path.insert(0, APP)

from psrx import protocollo as p  # noqa: E402
from psrx import servizio  # noqa: E402
from psrx.simulatore import PicoSimulato  # noqa: E402


class DeckyFinto(types.ModuleType):
    def __init__(self):
        super().__init__('decky')
        self.eventi = []
        self.logger = types.SimpleNamespace(info=lambda *a: None, error=lambda *a: None)
        self.DECKY_PLUGIN_SETTINGS_DIR = tempfile.mkdtemp(prefix='psrx-decky-')

    async def emit(self, nome, *argomenti):
        self.eventi.append((nome,) + argomenti)


def carica_backend(decky):
    sys.modules['decky'] = decky
    spec = importlib.util.spec_from_file_location('psrx_decky_main', os.path.join(APP, 'decky', 'main.py'))
    modulo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(modulo)
    return modulo


class TestBackendDecky(unittest.TestCase):
    def setUp(self):
        self.pico = PicoSimulato()
        self.decky = DeckyFinto()
        self.backend = carica_backend(self.decky)
        # Psrx() senza argomenti -> ricevitore simulato; niente regola udev (non siamo su SteamOS)
        self.backend.Psrx = lambda: servizio.Psrx(lambda: self.pico.trasporto)
        self.backend.permessi.regola_installata = lambda: True
        self.backend.INTERVALLO_NOTIFICHE_S = 0.01
        self.plugin = self.backend.Plugin()

    def test_flusso_completo(self):
        async def prova():
            await self.plugin._main()
            st = await self.plugin.stato()
            self.assertNotIn('errore', st)
            self.assertEqual(st['pad'][0]['mac'], '12:34:56:78:9A:BC')
            self.assertEqual(st['nome_modalita'], 'PlayStation')
            imp = await self.plugin.impostazioni()
            self.assertEqual(imp['valori'][str(p.IMP_BUFFER_AUDIO)], 64)
            self.assertTrue(all('sezione' in v for v in imp['schema']))
            self.assertEqual(await self.plugin.imposta(p.IMP_LED_POSTO, 1), {'ok': True})
            self.assertIn('errore', await self.plugin.imposta(p.IMP_BUFFER_AUDIO, 999))
            ab = await self.plugin.abbinati()
            self.assertEqual(ab['abbinati'][0]['posto'], 0)
            self.assertEqual(ab['abbinati'][0]['valori'][str(p.PAD_POLLING)], 0)
            mac = ab['abbinati'][0]['mac']
            self.assertEqual(await self.plugin.imposta_pad(mac, p.PAD_POLLING, 2), {'ok': True})
            self.assertIn('errore', await self.plugin.imposta_pad(mac, p.PAD_POLLING, 9))
            ab = await self.plugin.abbinati()
            self.assertEqual(ab['abbinati'][0]['valori'][str(p.PAD_POLLING)], 2)
            self.assertEqual(await self.plugin.rinomina(mac, 'Giocatore'), {'ok': True})
            reti = await self.plugin.reti()
            self.assertEqual(reti['reti'][0]['ssid'], 'Casa')
            self.assertEqual(await self.plugin.salva_rete(1, 'Ufficio', 'password123', False, False), {'ok': True})
            self.assertIn('errore', await self.plugin.salva_rete(2, 'Corta', '1234', False, False))
            self.assertEqual((await self.plugin.reti())['reti'][1]['ssid'], 'Ufficio')
            self.assertEqual(await self.plugin.destinazioni_wol('aa:bb:cc:dd:ee:ff', ''), {'ok': True})
            self.assertIn('errore', await self.plugin.destinazioni_wol('non-un-mac', ''))
            self.assertEqual((await self.plugin.reti())['wol_mac'][0], 'AA:BB:CC:DD:EE:FF')
            self.assertEqual(await self.plugin.spegni_pad(-1), {'ok': True})
            self.assertIn('attivo', await self.plugin.registro())
            await self.plugin._unload()
        asyncio.run(prova())

    def test_notifiche(self):
        async def prova():
            await self.plugin._main()
            await asyncio.sleep(0.1)          # primo giro: segna l'ultimo evento, niente notifiche
            self.pico.connetti_pad(1, p.mac_da_testo('A0:AB:51:00:11:22'), p.MODELLO_DUALSHOCK4, 15)
            self.pico.evento(p.EVENTO_COLLEGATO, 1)
            await asyncio.sleep(0.2)
            notifiche = [e for e in self.decky.eventi if e[0] == 'psrx_notifica']
            self.assertEqual(len(notifiche), 1)
            self.assertIn('Divano collegato', notifiche[0][1])
            self.assertIn('DualShock 4', notifiche[0][2])
            await self.plugin.imposta_notifiche(False)
            self.pico.evento(p.EVENTO_BATTERIA, 1, critico=True)
            await asyncio.sleep(0.2)
            self.assertEqual(len([e for e in self.decky.eventi if e[0] == 'psrx_notifica']), 1)
            await self.plugin._unload()
        asyncio.run(prova())


if __name__ == '__main__':
    unittest.main()
