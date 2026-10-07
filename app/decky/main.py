"""
PS-RX - backend del plugin Decky (gira come root: flag "root" in plugin.json).

Parla col ricevitore via USB con la libreria psrx (copiata in py_modules/ al momento della build) e la
espone al frontend (src/index.tsx). Le richieste USB sono brevi ma bloccanti: girano in un thread, una
alla volta. Il caricamento del firmware usa un client suo e manda l'avanzamento con l'evento
"psrx_avanzamento".

Notifiche (collegamento con posto, modello, modalita' e batteria; batteria in esaurimento): ogni 2 s,
anche a pannello chiuso, una lettura "silenziosa" dello stato (il Pico non la conta come app aperta e
non interroga la radio); solo con eventi nuovi si leggono eventi e nomi. Il frontend le mostra come
notifiche di Steam (evento "psrx_notifica").

Aggiornamenti: ricerca sulle release di GitHub (all'avvio al massimo una volta al giorno, con notifica, e a
richiesta), installazione del firmware scaricato e aggiornamento del plugin stesso (lo zip si estrae nella
cartella del plugin, poi Decky Loader si riavvia).

All'avvio installa la regola udev che permette anche alla riga di comando (senza root) di parlare col
ricevitore.
"""

import asyncio
import json
import os
import subprocess
import tempfile
import threading
import time
import zipfile

import decky

from psrx import VERSIONE_APP, aggiornamenti as ag, permessi, pico_nuovo, protocollo as p, schema
from psrx.errori import ErrorePsrx, PermessoNegato, Scollegato
from psrx.firmware import FirmwareNonValido, leggi as leggi_firmware
from psrx.notifiche import Sorvegliante
from psrx.servizio import Interrotto, Psrx

INTERVALLO_NOTIFICHE_S = 2
RITARDO_RICERCA_S = 30
INTERVALLO_RICERCA_S = 24 * 3600
CARTELLA_DOWNLOAD = os.path.join(tempfile.gettempdir(), 'ps-rx-aggiornamenti')
FILE_IMPOSTAZIONI = 'impostazioni.json'


def _errore(e: Exception) -> dict:
    if isinstance(e, Scollegato):
        return {'errore': 'Ricevitore PS-RX non trovato sull\'USB.'}
    if isinstance(e, PermessoNegato):
        return {'errore': 'Permesso negato sul nodo USB del ricevitore.'}
    if isinstance(e, ErrorePsrx):
        return {'errore': str(e), 'codice': e.codice}
    return {'errore': f'{type(e).__name__}: {e}'}


class Plugin:
    async def _main(self):
        self.loop = asyncio.get_running_loop()
        self.ps = Psrx()
        self.blocco = asyncio.Lock()
        self.caricatore = None
        self.ferma_caricamento = threading.Event()
        self.firmware = None
        self.sorvegliante = Sorvegliante(self.ps)
        self.opzioni = self._leggi_opzioni()
        self.sorveglianza = self.loop.create_task(self._sorveglia())
        self.controllo = None             # ultimo esito della ricerca degli aggiornamenti
        self.ricerca_automatica = self.loop.create_task(self._ricerca_all_avvio())
        ok, messaggio = await asyncio.to_thread(permessi.installa_come_root) if not permessi.regola_installata() \
            else (True, 'regola udev gia\' presente')
        decky.logger.info(f'PS-RX plugin {VERSIONE_APP}: {messaggio}')

    async def _unload(self):
        if self.caricatore:
            self.ferma_caricamento.set()
        self.sorveglianza.cancel()
        self.ricerca_automatica.cancel()
        self.ps.chiudi()

    # --- opzioni del plugin (non del ricevitore) ----------------------------------------------------
    def _percorso_opzioni(self) -> str:
        return os.path.join(getattr(decky, 'DECKY_PLUGIN_SETTINGS_DIR', '.'), FILE_IMPOSTAZIONI)

    def _leggi_opzioni(self) -> dict:
        opzioni = {'notifiche': True, 'cerca_aggiornamenti': True, 'ultima_ricerca': 0}
        try:
            with open(self._percorso_opzioni()) as f:
                opzioni.update(json.load(f))
        except (OSError, ValueError):
            pass
        return opzioni

    def _salva_opzioni(self) -> None:
        try:
            os.makedirs(os.path.dirname(self._percorso_opzioni()) or '.', exist_ok=True)
            with open(self._percorso_opzioni(), 'w') as f:
                json.dump(self.opzioni, f)
        except OSError as e:
            decky.logger.error(f'PS-RX: opzioni non salvate: {e}')

    async def notifiche(self) -> bool:
        return bool(self.opzioni.get('notifiche', True))

    async def imposta_notifiche(self, attive: bool) -> dict:
        self.opzioni['notifiche'] = bool(attive)
        self._salva_opzioni()
        return {'ok': True}

    async def cerca_aggiornamenti_attivo(self) -> bool:
        return bool(self.opzioni.get('cerca_aggiornamenti', True))

    async def imposta_cerca_aggiornamenti(self, attivo: bool) -> dict:
        self.opzioni['cerca_aggiornamenti'] = bool(attivo)
        self._salva_opzioni()
        return {'ok': True}

    # --- aggiornamenti da GitHub ----------------------------------------------------------------------
    async def _ricerca_all_avvio(self) -> None:
        await asyncio.sleep(RITARDO_RICERCA_S)
        if not self.opzioni.get('cerca_aggiornamenti', True):
            return
        if time.time() - float(self.opzioni.get('ultima_ricerca', 0) or 0) < INTERVALLO_RICERCA_S:
            return
        esito = await self.aggiornamenti()
        if esito.get('firmware_nuovo') or esito.get('plugin_nuovo'):
            cosa = ' e '.join(x for x, s in (('firmware', esito['firmware_nuovo']), ('plugin', esito['plugin_nuovo'])) if s)
            await decky.emit('psrx_notifica', 'PS-RX: aggiornamento disponibile',
                             f'Versione {esito["versione"]} ({cosa}): menu di PS-RX, sezione Sistema.', False)

    async def aggiornamenti(self) -> dict:
        firmware = None
        try:
            firmware = (await self._usb(self.ps.info)).versione
        except Exception:  # noqa: BLE001 - senza ricevitore si controlla solo il plugin
            self.ps.chiudi()
        try:
            c = await asyncio.to_thread(ag.controlla, firmware, VERSIONE_APP, 'decky')
        except ag.ErroreAggiornamento as e:
            return {'errore': str(e)}
        self.controllo = c
        self.opzioni['ultima_ricerca'] = time.time()
        self._salva_opzioni()
        return {'versione': c.release.versione, 'pagina': c.release.pagina, 'note': c.release.note[:600],
                'firmware_installato': firmware or '', 'firmware_nuovo': c.firmware_nuovo,
                'plugin_installato': VERSIONE_APP, 'plugin_nuovo': c.app_nuova}

    async def installa_firmware_github(self) -> dict:
        if self.controllo is None or not self.controllo.firmware_nuovo:
            return {'errore': 'cerca prima gli aggiornamenti'}
        f = ag.file_firmware(self.controllo.release)
        try:
            percorso = await asyncio.to_thread(ag.scarica, f, CARTELLA_DOWNLOAD)
        except ag.ErroreAggiornamento as e:
            return {'errore': str(e)}
        r = await self.prepara_firmware(percorso)
        if r.get('errore'):
            return r
        return await self.carica_firmware()

    async def aggiorna_plugin(self) -> dict:
        if self.controllo is None or not self.controllo.app_nuova:
            return {'errore': 'cerca prima gli aggiornamenti'}
        cartella = getattr(decky, 'DECKY_PLUGIN_DIR', '')
        if not cartella or not os.path.isdir(cartella):
            return {'errore': 'cartella del plugin non trovata'}
        try:
            zip_ = await asyncio.to_thread(ag.scarica, self.controllo.release.file['decky'], CARTELLA_DOWNLOAD)
            await asyncio.to_thread(estrai_plugin, zip_, cartella)
        except (ag.ErroreAggiornamento, OSError, zipfile.BadZipFile, ValueError) as e:
            return {'errore': f'aggiornamento del plugin non riuscito: {e}'}
        decky.logger.info(f'PS-RX: plugin aggiornato a {self.controllo.release.versione}, riavvio Decky Loader')
        # Riavvio di Decky Loader poco dopo, per lasciare il tempo alla risposta di arrivare al pannello.
        self.loop.call_later(2, lambda: subprocess.Popen(['systemctl', 'restart', 'plugin_loader']))
        return {'ok': True}

    # --- nuovo ricevitore (Pico 2 W nuovo o in BOOTSEL) ------------------------------------------------
    async def pico_bootsel(self) -> bool:
        return await asyncio.to_thread(pico_nuovo.presente)

    async def installa_pico(self) -> dict:
        """Ultimo firmware da GitHub (o quello incluso nel plugin se non c'e' internet) copiato nel Pico in
        BOOTSEL; il plugin gira come root e monta da se' la chiavetta. Poi si aspetta il PS-RX."""
        if self.caricatore is not None:
            return {'errore': 'un aggiornamento e\' gia\' in corso'}
        if not await asyncio.to_thread(pico_nuovo.presente):
            return {'errore': 'nessun Pico in modalita\' BOOTSEL: collegalo tenendo premuto BOOTSEL'}
        origine = ''
        try:
            rel = await asyncio.to_thread(ag.ultima_release)
            f = rel.file.get('firmware')
            if f is None:
                raise ag.ErroreAggiornamento('release senza firmware')
            percorso = await asyncio.to_thread(ag.scarica, f, CARTELLA_DOWNLOAD)
            origine = f'{rel.versione} da GitHub'
        except ag.ErroreAggiornamento:
            percorso = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'firmware', 'ps-rx-firmware.uf2')
            if not os.path.exists(percorso):
                return {'errore': 'GitHub non raggiungibile e nessun firmware incluso nel plugin'}
            origine = 'incluso nel plugin'
        try:
            img = await asyncio.to_thread(leggi_firmware, percorso)
            await asyncio.to_thread(pico_nuovo.scrivi_uf2_come_root, percorso)
            self.ps.chiudi()
            t = await asyncio.to_thread(pico_nuovo.attendi_psrx, ag_apri_usb)
            t.chiudi()
        except (OSError, FirmwareNonValido, TimeoutError, subprocess.SubprocessError) as e:
            return {'errore': f'installazione non riuscita: {e}'}
        decky.logger.info(f'PS-RX: firmware {img.versione} ({origine}) installato su un Pico nuovo')
        return {'ok': True, 'versione': img.versione or '', 'origine': origine}

    # --- notifiche ------------------------------------------------------------------------------------
    async def _sorveglia(self) -> None:
        while True:
            await asyncio.sleep(INTERVALLO_NOTIFICHE_S)
            if self.caricatore is not None:
                continue
            try:
                nuove = await self._usb(self.sorvegliante.controlla)
            except Exception:  # noqa: BLE001 - ricevitore assente o in riavvio: si riprova
                self.ps.chiudi()
                continue
            if not self.opzioni.get('notifiche', True):
                continue
            for titolo, testo, critica in nuove:
                decky.logger.info(f'PS-RX: {titolo}: {testo}')
                await decky.emit('psrx_notifica', titolo, testo, critica)

    async def _usb(self, funzione, *argomenti):
        """Una richiesta USB alla volta, fuori dal ciclo di eventi."""
        async with self.blocco:
            return await asyncio.to_thread(funzione, *argomenti)

    async def _azione(self, funzione, *argomenti) -> dict:
        try:
            await self._usb(funzione, *argomenti)
            return {'ok': True}
        except Exception as e:  # noqa: BLE001
            return _errore(e)

    # --- letture --------------------------------------------------------------------------------------
    async def stato(self) -> dict:
        try:
            info = await self._usb(self.ps.info)
            st = await self._usb(self.ps.stato)
            risultato = {
                'versione': info.versione, 'base': info.base, 'modalita': st.modalita,
                'nome_modalita': st.nome_modalita, 'usb_gamepad': st.usb_gamepad, 'usb_sospeso': st.usb_sospeso,
                'posti_fissi': st.posti_fissi, 'finestra_abbinamento': st.finestra_abbinamento,
                'altoparlante': st.altoparlante, 'microfono': st.microfono, 'microfono_muto': st.microfono_muto,
                'salvataggio_in_sospeso': st.salvataggio_in_sospeso, 'pad_connessi': st.pad_connessi,
                'uptime_s': st.uptime_s, 'heap_libero': st.heap_libero,
                'rete': p.TESTO_RETE.get(st.rete_stato, '?'), 'rete_connessa': st.rete_stato == p.RETE_CONNESSA,
                'rete_ip': st.rete_ip, 'rete_rssi': st.rete_rssi, 'ms_da_ultimo_wol': st.ms_da_ultimo_wol,
                'pad': [{'posto': x.posto, 'modello': x.nome_modello, 'mac': x.mac, 'batteria': x.batteria,
                         'batteria_valida': x.batteria_valida, 'in_carica': x.in_carica, 'rssi': x.rssi,
                         'report': x.report_al_secondo, 'hz': p.POLLING_HZ.get(x.polling, 0), 'audio': x.audio}
                        for x in st.pad if x.connesso],
                'caricamento': st.caricamento, 'caricamento_testo': '',
                'caricatore_attivo': self.caricatore is not None,
            }
            if st.caricamento != p.CAR_INATTIVO:
                risultato['caricamento_testo'] = (await self._usb(self.ps.caricamento)).descrizione
            return risultato
        except Exception as e:  # noqa: BLE001
            self.ps.chiudi()
            return _errore(e)

    async def impostazioni(self) -> dict:
        try:
            valori = await self._usb(self.ps.impostazioni)
            return {'schema': schema.IMPOSTAZIONI, 'valori': {str(k): v for k, v in valori.items()}}
        except Exception as e:  # noqa: BLE001
            return _errore(e)

    async def abbinati(self) -> dict:
        try:
            lista = await self._usb(self.ps.abbinati)
            return {'schema': schema.IMPOSTAZIONI_PAD,
                    'abbinati': [{'mac': a.mac, 'nome': a.nome, 'posto': a.posto,
                                  'valori': {str(v['id']): int(getattr(a, v['chiave']))
                                             for v in schema.IMPOSTAZIONI_PAD}} for a in lista]}
        except Exception as e:  # noqa: BLE001
            return _errore(e)

    async def reti(self) -> dict:
        try:
            r = await self._usb(self.ps.reti)
            return {'reti': [{'indice': x.indice, 'ssid': x.ssid, 'wpa3': x.wpa3, 'ha_password': x.ha_password}
                             for x in r.reti], 'wol_mac': r.wol_mac}
        except Exception as e:  # noqa: BLE001
            return _errore(e)

    async def registro(self) -> dict:
        try:
            attivo, testo = await self._usb(self.ps.registro)
            return {'attivo': attivo, 'testo': testo}
        except Exception as e:  # noqa: BLE001
            return _errore(e)

    # --- azioni ---------------------------------------------------------------------------------------
    async def imposta(self, ident: int, valore: int) -> dict:
        if not schema.valido(int(ident), int(valore)):
            return {'errore': 'valore non valido'}
        return await self._azione(self.ps.imposta, int(ident), int(valore))

    async def imposta_pad(self, mac: str, ident: int, valore: int) -> dict:
        if not schema.valido_pad(int(ident), int(valore)):
            return {'errore': 'valore non valido'}
        return await self._azione(self.ps.imposta_pad, mac, int(ident), int(valore))

    async def salva_ora(self) -> dict:
        return await self._azione(self.ps.salva_ora)

    async def predefinite(self) -> dict:
        return await self._azione(self.ps.predefinite)

    async def rinomina(self, mac: str, nome: str) -> dict:
        return await self._azione(self.ps.rinomina, mac, nome)

    async def dimentica(self, mac: str) -> dict:
        return await self._azione(self.ps.dimentica, mac)

    async def dimentica_tutti(self) -> dict:
        return await self._azione(self.ps.dimentica_tutti)

    async def abbina(self) -> dict:
        return await self._azione(self.ps.abbina)

    async def spegni_pad(self, posto: int = -1) -> dict:
        return await self._azione(self.ps.spegni_pad, None if posto is None or int(posto) < 0 else int(posto))

    async def salva_rete(self, indice: int, ssid: str, password: str, wpa3: bool, mantieni: bool) -> dict:
        ssid = str(ssid)
        password = str(password)
        if not ssid or len(ssid.encode('utf-8')) > 32:
            return {'errore': 'nome della rete vuoto o troppo lungo (massimo 32 byte)'}
        if password and not 8 <= len(password.encode('utf-8')) <= 63:
            return {'errore': 'la password WPA va da 8 a 63 caratteri'}
        return await self._azione(self.ps.salva_rete, int(indice), ssid, password, bool(wpa3), bool(mantieni))

    async def cancella_rete(self, indice: int) -> dict:
        return await self._azione(self.ps.cancella_rete, int(indice))

    async def destinazioni_wol(self, mac1: str, mac2: str) -> dict:
        try:
            for m in (mac1, mac2):
                if m:
                    p.mac_da_testo(m)
        except ValueError:
            return {'errore': 'MAC non valido (formato AA:BB:CC:DD:EE:FF)'}
        return await self._azione(self.ps.destinazioni_wol, mac1.upper(), mac2.upper())

    async def cerca_reti(self) -> dict:
        return await self._azione(self.ps.cerca_reti)

    async def reti_viste(self) -> dict:
        try:
            v = await self._usb(self.ps.reti_viste)
        except Exception as ex:  # noqa: BLE001
            return _errore(ex)
        return {'finita': v.finita, 'annullata': v.stato == p.SCAN_ANNULLATA,
                'reti': [{'ssid': r.ssid, 'rssi': r.rssi, 'canale': r.canale, 'aperta': r.aperta, 'tacche': r.tacche}
                         for r in v.reti]}

    async def prova_rete(self, indice: int) -> dict:
        return await self._azione(self.ps.prova_rete, int(indice))

    async def esito_prova_rete(self) -> dict:
        try:
            e = await self._usb(self.ps.esito_prova_rete)
        except Exception as ex:  # noqa: BLE001
            return _errore(ex)
        return {'fase': e.fase, 'esito': e.esito, 'finita': e.finita, 'ok': e.esito == p.ESITO_OK,
                'rete': e.rete, 'descrizione': e.descrizione}

    async def prova_wol(self) -> dict:
        return await self._azione(self.ps.prova_wol)

    async def bootsel(self) -> dict:
        return await self._azione(self.ps.bootsel_ora)

    # --- aggiornamento del firmware -------------------------------------------------------------------
    async def prepara_firmware(self, percorso: str) -> dict:
        try:
            info = await self._usb(self.ps.info)
            self.firmware = await asyncio.to_thread(leggi_firmware, percorso, info.staging_max)
            return {'nome': self.firmware.nome, 'versione': self.firmware.versione or 'sconosciuta',
                    'dimensione': self.firmware.dimensione}
        except FirmwareNonValido as e:
            self.firmware = None
            return {'errore': f'File non valido: {e}'}
        except Exception as e:  # noqa: BLE001
            self.firmware = None
            return _errore(e)

    async def carica_firmware(self) -> dict:
        if self.firmware is None:
            return {'errore': 'scegli prima il file del firmware'}
        if self.caricatore is not None:
            return {'errore': 'caricamento gia\' in corso'}
        self.ferma_caricamento.clear()
        self.caricatore = self.loop.create_task(self._carica(self.firmware))
        return {'ok': True}

    async def annulla_caricamento(self) -> dict:
        self.ferma_caricamento.set()
        return {'ok': True}

    def _emetti(self, *argomenti) -> None:
        asyncio.run_coroutine_threadsafe(decky.emit('psrx_avanzamento', *argomenti), self.loop)

    async def _carica(self, img) -> None:
        def lavoro():
            ps = Psrx()        # client suo: lo stato del pannello continua ad aggiornarsi
            try:
                ps.carica_firmware(img, lambda fase, fatto, totale: self._emetti(fase, fatto, totale, ''),
                                   self.ferma_caricamento.is_set)
                return True, 'Firmware installato: il ricevitore si riavvia.'
            except Interrotto:
                return False, 'Caricamento annullato.'
            except Exception as e:  # noqa: BLE001
                return False, 'Aggiornamento non riuscito: ' + _errore(e)['errore']
            finally:
                ps.chiudi()

        try:
            ok, messaggio = await asyncio.to_thread(lavoro)
        finally:
            self.caricatore = None
            self.ps.chiudi()
        decky.logger.info(f'PS-RX aggiornamento del firmware: {messaggio}')
        await decky.emit('psrx_avanzamento', 'fine' if ok else 'errore', 0, 0, messaggio)


def estrai_plugin(zip_: str, cartella: str) -> None:
    """Estrae lo zip del plugin (cartella ps-rx-decky/ al suo interno) sopra la cartella del plugin installato.
    Prima controlla tutti i percorsi: niente file fuori dalla cartella."""
    radice = os.path.realpath(cartella)
    with zipfile.ZipFile(zip_) as z:
        voci = []
        for info in z.infolist():
            parti = info.filename.split('/', 1)
            if len(parti) < 2 or not parti[1] or info.is_dir():
                continue
            destinazione = os.path.realpath(os.path.join(radice, parti[1]))
            if not destinazione.startswith(radice + os.sep):
                raise ValueError(f'percorso non ammesso nello zip: {info.filename}')
            voci.append((info, destinazione))
        if not any(d.endswith(os.sep + 'plugin.json') for _, d in voci):
            raise ValueError('lo zip non contiene un plugin')
        for info, destinazione in voci:
            os.makedirs(os.path.dirname(destinazione), exist_ok=True)
            with z.open(info) as sorgente, open(destinazione + '.nuovo', 'wb') as out:
                out.write(sorgente.read())
            os.replace(destinazione + '.nuovo', destinazione)


def ag_apri_usb():
    from psrx.servizio import apri_usb
    return apri_usb()
