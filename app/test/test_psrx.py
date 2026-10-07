"""Test della libreria psrx con il ricevitore simulato e del contratto con il firmware."""

import io
import os
import re
import struct
import sys
import unittest
from contextlib import redirect_stdout

QUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(QUI))

from psrx import firmware, protocollo as p, schema  # noqa: E402
from psrx.errori import ErrorePsrx  # noqa: E402
from psrx.notifiche import Sorvegliante  # noqa: E402
from psrx.servizio import Psrx  # noqa: E402
from psrx.simulatore import PicoSimulato  # noqa: E402

REPO = os.path.dirname(os.path.dirname(QUI))
HEADER = os.path.join(REPO, 'firmware', 'src', 'psrx', 'protocollo.h')
CONFIG_H = os.path.join(REPO, 'firmware', 'src', 'config.h')


def immagine_finta(dim=70_000):
    """Immagine che passa i controlli di firmware.problemi(): vettori e blocco IMAGE_DEF."""
    b = bytearray(b'\xff' * dim)
    struct.pack_into('<II', b, 0, 0x20082000, 0x1000015D)
    struct.pack_into('<I', b, 312, 0xFFFFDED3)
    struct.pack_into('<I', b, 328, 0xAB123579)
    b[1000:1000 + 16] = b'ps-rx-9.9.9-prova'[:16]
    return bytes(b)


class TestContratto(unittest.TestCase):
    """protocollo.py deve coincidere con protocollo.h del firmware."""

    @classmethod
    def setUpClass(cls):
        with open(HEADER, encoding='utf-8') as f:
            cls.h = f.read()

    def valore_h(self, nome):
        m = re.search(rf'\b{nome}\s*=\s*(0x[0-9A-Fa-f]+|\d+)', self.h)
        self.assertIsNotNone(m, nome)
        return int(m.group(1), 0)

    def test_costanti(self):
        nomi = re.findall(r'\b((?:CMD|ERR|IMP|PAD|MODELLO|CAR|EVENTO)_[A-Z_]+)\s*=', self.h)
        self.assertGreater(len(nomi), 60)
        for nome in nomi:
            self.assertEqual(getattr(p, nome), self.valore_h(nome), nome)
        for nome, h in (('RICHIESTA', 'PSRX_RICHIESTA'), ('PROTOCOLLO', 'PSRX_PROTOCOLLO'),
                        ('SLOT_TUTTI', 'PSRX_SLOT_TUTTI'), ('STATO_SILENZIOSO', 'STATO_SILENZIOSO'),
                        ('PAD_MAX', 'PSRX_PAD_MAX'), ('RETI_MAX', 'PSRX_RETI_MAX'),
                        ('N_IMPOSTAZIONI', 'PSRX_N_IMPOSTAZIONI')):
            self.assertEqual(getattr(p, nome), self.valore_h(h), nome)

    def test_capacita(self):
        for nome, bit in re.findall(r'\b(CAP_[A-Z_]+)\s*=\s*1u << (\d+)', self.h):
            self.assertEqual(getattr(p, nome), 1 << int(bit), nome)

    def test_dimensioni(self):
        dimensioni = dict(re.findall(r'sizeof\((\w+)\) == (\d+),', self.h))
        attese = {'InfoPsrx': p.DIM_INFO, 'PadPsrx': p.DIM_PAD, 'VoceImpostazione': 3,
                  'VoceAbbinato': p.DIM_ABBINATO, 'ImpostaPad': struct.calcsize(p.FMT_IMPOSTA_PAD),
                  'EventoPsrx': p.DIM_EVENTO, 'VoceRete': p.DIM_VOCE_RETE,
                  'ReteDati': struct.calcsize(p.FMT_RETE_DATI), 'CaricamentoPsrx': struct.calcsize(p.FMT_CARICAMENTO),
                  'CaricaInizio': struct.calcsize(p.FMT_CARICA_INIZIO)}
        for nome, dim in attese.items():
            self.assertEqual(int(dimensioni[nome]), dim, nome)
        self.assertIn('sizeof(StatoPsrx) == 32 + PSRX_PAD_MAX * sizeof(PadPsrx)', self.h)
        self.assertEqual(struct.calcsize(p.FMT_STATO), 32)

    def test_schema_completo(self):
        self.assertEqual(sorted(schema.PER_ID), list(range(1, p.N_IMPOSTAZIONI + 1)))
        self.assertEqual(sorted(schema.PAD_PER_ID), [1, 2, 3, 4, 5])

    def test_limiti_della_configurazione(self):
        with open(CONFIG_H, encoding='utf-8') as f:
            c = f.read()
        self.assertIn(f'#define PSRX_MAX_RETI           {p.RETI_MAX}', c)
        self.assertIn(f'#define PSRX_MAX_PAD            {p.PAD_MAX}', c)


class TestFirmware(unittest.TestCase):
    def test_uf2(self):
        dati = immagine_finta()
        blocchi = []
        for i in range(0, len(dati), 256):
            pezzo = dati[i:i + 256].ljust(476, b'\0')
            blocchi.append(struct.pack('<IIIIIIII', 0x0A324655, 0x9E5D5157, 0x2000, 0x10000000 + i, 256,
                                       i // 256, (len(dati) + 255) // 256, 0xE48BFF59) + pezzo
                           + struct.pack('<I', 0x0AB16F30))
        uf2 = b''.join(blocchi)
        self.assertEqual(firmware.da_uf2(uf2)[:len(dati)], dati)

    def test_problemi(self):
        self.assertEqual(firmware.problemi(immagine_finta(), 2_000_000), [])
        self.assertTrue(firmware.problemi(b'\0' * 100_000, 2_000_000))


class TestServizio(unittest.TestCase):
    def setUp(self):
        self.pico = PicoSimulato()
        self.ps = Psrx(lambda: self.pico.trasporto)

    def test_info_e_stato(self):
        info = self.ps.info()
        self.assertEqual(info.versione, 'ps-rx-0.1.0-simulato')
        st = self.ps.stato()
        self.assertEqual(st.nome_modalita, 'PlayStation')
        self.assertEqual(st.pad_connessi, 1)
        self.assertEqual(st.pad[0].mac, '12:34:56:78:9A:BC')
        self.assertEqual(st.pad[0].nome_modello, 'DualSense')
        self.assertTrue(st.pad[0].audio)
        self.assertEqual(st.pad[0].report_al_secondo, 250)

    def test_stato_silenzioso(self):
        self.ps.stato()
        self.ps.stato(silenzioso=True)
        self.assertEqual(self.pico.stati_app, 1)

    def test_impostazioni(self):
        self.ps.imposta(p.IMP_MODALITA, 1)
        self.assertEqual(self.ps.impostazioni()[p.IMP_MODALITA], 1)
        self.assertEqual(self.ps.stato().nome_modalita, 'Xbox')
        with self.assertRaises(ErrorePsrx) as e:
            self.ps.imposta(p.IMP_MODALITA, 3)
        self.assertEqual(e.exception.codice, p.ERR_VALORE)

    def test_impostazioni_per_controller(self):
        mac = '12:34:56:78:9A:BC'
        self.ps.imposta_pad(mac, p.PAD_POLLING, 2)
        self.ps.imposta_pad(mac, p.PAD_AUDIO, 0)
        self.ps.imposta_pad(mac, p.PAD_TRACKPAD, 1)
        ab = next(a for a in self.ps.abbinati() if a.mac == mac)
        self.assertEqual((ab.polling, ab.audio, ab.trackpad), (2, False, True))
        self.assertFalse(self.ps.stato().pad[0].audio)
        with self.assertRaises(ErrorePsrx) as e:
            self.ps.imposta_pad('00:00:00:00:00:01', p.PAD_AUDIO, 1)
        self.assertEqual(e.exception.codice, p.ERR_NON_TROVATO)

    def test_abbinati(self):
        self.ps.rinomina('12:34:56:78:9A:BC', 'Giocatore èè')
        lista = self.ps.abbinati()
        self.assertEqual(lista[0].nome, 'Giocatore èè')
        self.assertEqual(lista[0].posto, 0)
        self.assertIsNone(lista[1].posto)
        self.ps.dimentica('A0:AB:51:00:11:22')
        self.assertEqual(len(self.ps.abbinati()), 1)

    def test_reti_e_wol(self):
        self.ps.salva_rete(1, 'Ufficio', 'segretissima', wpa3=True)
        r = self.ps.reti()
        self.assertEqual((r.reti[1].ssid, r.reti[1].wpa3, r.reti[1].ha_password), ('Ufficio', True, True))
        self.ps.salva_rete(1, 'Ufficio', '', wpa3=True, mantieni_password=True)
        self.assertTrue(self.ps.reti().reti[1].ha_password)
        with self.assertRaises(ErrorePsrx):
            self.ps.salva_rete(2, 'Corta', 'abc')            # password sotto gli 8 caratteri
        self.ps.cancella_rete(1)
        self.assertEqual(self.ps.reti().reti[1].ssid, '')
        self.ps.destinazioni_wol('AA:BB:CC:DD:EE:FF')
        self.assertEqual(self.ps.reti().wol_mac, ['AA:BB:CC:DD:EE:FF', ''])
        with self.assertRaises(ErrorePsrx) as e:
            self.ps.prova_wol()
        self.assertEqual(e.exception.codice, p.ERR_NON_CONNESSO)
        self.pico.rete_stato = p.RETE_CONNESSA
        self.ps.prova_wol()
        self.assertEqual(self.pico.wol_inviati, 1)
        self.assertEqual(self.ps.stato().rete_ip, '192.168.1.50')

    def test_notifiche(self):
        s = Sorvegliante(self.ps)
        self.assertEqual(s.controlla(), [])                     # all'avvio niente notifiche vecchie
        self.pico.connetti_pad(1, p.mac_da_testo('A0:AB:51:00:11:22'), p.MODELLO_DS4, 60)
        self.pico.evento(p.EVENTO_COLLEGATO, 1)
        n = s.controlla()
        self.assertEqual(len(n), 1)
        titolo, testo, critica = n[0]
        self.assertEqual(titolo, 'Divano collegato')
        self.assertIn('Posto 2 · DualShock 4 · modalità PlayStation · batteria 60%', testo)
        self.assertFalse(critica)
        self.assertEqual(s.controlla(), [])
        self.pico.pad[1]['batteria'] = 10
        self.pico.evento(p.EVENTO_BATTERIA, 1, critico=True)
        titolo, testo, critica = s.controlla()[0]
        self.assertEqual(titolo, 'Batteria quasi scarica')
        self.assertTrue(critica)
        # il ricevitore si riavvia: la numerazione riparte da capo
        self.pico.eventi.clear()
        self.pico.numero_evento = 0
        self.pico.evento(p.EVENTO_COLLEGATO, 0)
        self.assertEqual(len(s.controlla()), 1)

    def test_carica_firmware(self):
        dati = immagine_finta()
        img = firmware.Immagine('prova.bin', dati, __import__('hashlib').sha256(dati).digest(), 'ps-rx-9.9.9')
        fasi = []
        self.pico.scollega_pad(0)
        self.ps.carica_firmware(img, lambda fase, fatto, totale: fasi.append(fase))
        self.assertEqual(self.pico.installato, dati)
        self.assertIn('caricamento', fasi)
        self.assertEqual(fasi[-1], 'installazione')

    def test_carica_con_controller_aspetta(self):
        dati = immagine_finta()
        img = firmware.Immagine('prova.bin', dati, __import__('hashlib').sha256(dati).digest(), None)
        fasi = []

        def avanzamento(fase, fatto, totale):
            fasi.append(fase)
            if fase == 'attesa_pad':
                self.pico.scollega_pad(0)   # l'utente spegne il controller
        import psrx.servizio as servizio
        dorme = servizio.time.sleep
        servizio.time.sleep = lambda s: None
        try:
            self.ps.carica_firmware(img, avanzamento)
        finally:
            servizio.time.sleep = dorme
        self.assertEqual(fasi[0], 'attesa_pad')
        self.assertEqual(self.pico.installato, dati)


class TestCli(unittest.TestCase):
    def esegui(self, *argomenti):
        from psrx.cli import main
        f = io.StringIO()
        with redirect_stdout(f):
            codice = main(['--simulatore', *argomenti])
        return codice, f.getvalue()

    def test_stato(self):
        codice, testo = self.esegui('stato')
        self.assertEqual(codice, 0)
        self.assertIn('Modalità: PlayStation', testo)
        self.assertIn('posto 1: DualSense 12:34:56:78:9A:BC', testo)

    def test_impostazioni_e_abbinati(self):
        self.assertIn('Modalità del ricevitore', self.esegui('impostazioni')[1])
        self.assertIn('Divano', self.esegui('abbinati')[1])
        self.assertEqual(self.esegui('imposta', 'modalita', '5')[0], 1)

    def test_regola_udev(self):
        codice, testo = self.esegui('regola-udev')
        self.assertEqual(codice, 0)
        self.assertIn('ATTR{idVendor}=="054c", ATTR{idProduct}=="0ce6"', testo)
        self.assertIn('ATTR{idVendor}=="045e", ATTR{idProduct}=="028e"', testo)


if __name__ == '__main__':
    unittest.main()
