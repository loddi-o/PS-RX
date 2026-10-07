"""
PS-RX - aggiornamenti dalle release di GitHub (github.com/loddi-o/PS-RX), solo libreria standard.

Ogni release ha il tag vX.Y.Z e questi file (stesso numero di versione per tutto):
    ps-rx-firmware-X.Y.Z.uf2     firmware del Pico (anche ps-rx-firmware-X.Y.Z.bin)
    PS-RX-Setup-X.Y.Z.exe        installer dell'app Windows (anche per aggiornarla)
    ps-rx-decky-X.Y.Z.zip        plugin Decky
    ps-rx-X.Y.Z.html             pagina WebUSB

Le richieste vanno all'API pubblica di GitHub (nessun token: la repo e' pubblica, limite di 60 richieste
all'ora per indirizzo IP, ampiamente sufficiente). Il download controlla l'impronta SHA-256 quando GitHub
la fornisce.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from typing import Callable, List, Optional, Tuple

from . import VERSIONE_APP

REPO = 'loddi-o/PS-RX'
URL_ULTIMA = f'https://api.github.com/repos/{REPO}/releases/latest'
URL_PAGINA = f'https://github.com/{REPO}/releases/latest'
TIMEOUT_S = 15

MODELLI = {
    'firmware': re.compile(r'^ps-rx-firmware-[0-9][^/]*\.uf2$'),
    'firmware_bin': re.compile(r'^ps-rx-firmware-[0-9][^/]*\.bin$'),
    'app_windows': re.compile(r'^PS-RX-Setup-[0-9][^/]*\.exe$'),
    'decky': re.compile(r'^ps-rx-decky-[0-9][^/]*\.zip$'),
    'pagina': re.compile(r'^ps-rx-[0-9][^/]*\.html$'),
}


class ErroreAggiornamento(Exception):
    pass


@dataclass
class File:
    nome: str
    url: str
    dimensione: int
    sha256: Optional[str]   # esadecimale, se GitHub la fornisce


@dataclass
class Release:
    versione: str            # 'X.Y.Z'
    tag: str
    titolo: str
    note: str
    pagina: str
    file: dict = field(default_factory=dict)   # tipo (vedi MODELLI) -> File


def numero(versione: Optional[str]) -> Optional[Tuple[int, ...]]:
    """'ps-rx-0.2.1', 'v0.2.1', '0.2.1' -> (0, 2, 1); None se non e' un numero di versione (es. 'ps-rx-dev')."""
    if not versione:
        return None
    m = re.search(r'(\d+)\.(\d+)(?:\.(\d+))?', versione)
    if not m:
        return None
    return tuple(int(x or 0) for x in m.groups())


def piu_nuova(disponibile: str, installata: Optional[str]) -> bool:
    """True se la versione disponibile e' piu' recente. Una versione installata senza numero (build di
    sviluppo 'ps-rx-dev') conta come piu' vecchia di qualsiasi release."""
    d = numero(disponibile)
    if d is None:
        return False
    i = numero(installata)
    return i is None or d > i


def _richiesta(url: str, accetta: str = 'application/vnd.github+json'):
    req = urllib.request.Request(url, headers={'Accept': accetta, 'User-Agent': f'PS-RX/{VERSIONE_APP}',
                                               'X-GitHub-Api-Version': '2022-11-28'})
    return urllib.request.urlopen(req, timeout=TIMEOUT_S)  # noqa: S310 - solo https verso GitHub


def ultima_release() -> Release:
    """Ultima release pubblicata (non le prerelease). Solleva ErroreAggiornamento se GitHub non risponde."""
    try:
        with _richiesta(URL_ULTIMA) as r:
            dati = json.load(r)
    except urllib.error.HTTPError as e:
        if e.code == 404:
            raise ErroreAggiornamento('nessuna release pubblicata su GitHub') from e
        if e.code == 403:
            raise ErroreAggiornamento('GitHub ha limitato le richieste: riprova tra un\'ora') from e
        raise ErroreAggiornamento(f'GitHub ha risposto con l\'errore {e.code}') from e
    except (urllib.error.URLError, OSError, ValueError) as e:
        raise ErroreAggiornamento(f'GitHub non raggiungibile ({e})') from e
    return da_json(dati)


def da_json(dati: dict) -> Release:
    tag = dati.get('tag_name', '')
    rel = Release(versione='.'.join(str(x) for x in (numero(tag) or ())), tag=tag, titolo=dati.get('name') or tag,
                  note=dati.get('body') or '', pagina=dati.get('html_url') or URL_PAGINA)
    for a in dati.get('assets', []):
        nome = a.get('name', '')
        digest = a.get('digest') or ''
        sha = digest.split(':', 1)[1] if digest.startswith('sha256:') else None
        for tipo, modello in MODELLI.items():
            if modello.match(nome) and tipo not in rel.file:
                rel.file[tipo] = File(nome, a.get('browser_download_url', ''), int(a.get('size') or 0), sha)
    return rel


def scarica(f: File, cartella: str, avanzamento: Callable[[int, int], None] = lambda fatti, totali: None,
            interrotto: Callable[[], bool] = lambda: False) -> str:
    """Scarica il file nella cartella e ne controlla dimensione e impronta. Restituisce il percorso."""
    os.makedirs(cartella, exist_ok=True)
    destinazione = os.path.join(cartella, f.nome)
    temporaneo = destinazione + '.parziale'
    impronta = hashlib.sha256()
    fatti = 0
    try:
        with _richiesta(f.url, 'application/octet-stream') as r, open(temporaneo, 'wb') as out:
            totali = int(r.headers.get('Content-Length') or f.dimensione or 0)
            while True:
                if interrotto():
                    raise ErroreAggiornamento('download annullato')
                pezzo = r.read(64 * 1024)
                if not pezzo:
                    break
                out.write(pezzo)
                impronta.update(pezzo)
                fatti += len(pezzo)
                avanzamento(fatti, totali)
    except (urllib.error.URLError, OSError) as e:
        _togli(temporaneo)
        raise ErroreAggiornamento(f'download non riuscito ({e})') from e
    except ErroreAggiornamento:
        _togli(temporaneo)
        raise
    if f.dimensione and fatti != f.dimensione:
        _togli(temporaneo)
        raise ErroreAggiornamento(f'download incompleto ({fatti} di {f.dimensione} byte)')
    if f.sha256 and impronta.hexdigest() != f.sha256.lower():
        _togli(temporaneo)
        raise ErroreAggiornamento('impronta SHA-256 diversa da quella pubblicata: file rovinato')
    os.replace(temporaneo, destinazione)
    return destinazione


def _togli(percorso: str) -> None:
    try:
        os.remove(percorso)
    except OSError:
        pass


@dataclass
class Controllo:
    """Esito di una ricerca: cosa c'e' di nuovo rispetto a quello installato."""
    release: Release
    firmware_nuovo: bool
    app_nuova: bool
    firmware_installato: Optional[str]
    app_installata: str

    @property
    def qualcosa(self) -> bool:
        return self.firmware_nuovo or self.app_nuova


def controlla(firmware_installato: Optional[str], app_installata: str = VERSIONE_APP,
              tipo_app: str = 'app_windows') -> Controllo:
    rel = ultima_release()
    # firmware_installato None = ricevitore non collegato: non si sa, non si propone nulla. Una build di
    # sviluppo ('ps-rx-dev') invece conta come piu' vecchia di qualsiasi release.
    fw = (firmware_installato is not None and piu_nuova(rel.versione, firmware_installato)
          and file_firmware(rel) is not None)
    app = piu_nuova(rel.versione, app_installata) and tipo_app in rel.file
    return Controllo(rel, fw, app, firmware_installato, app_installata)


def file_firmware(rel: Release) -> Optional[File]:
    return rel.file.get('firmware') or rel.file.get('firmware_bin')


def riepilogo(c: Controllo) -> List[str]:
    righe = [f'Ultima release: {c.release.titolo} ({c.release.versione})']
    fw = c.firmware_installato or 'sconosciuto'
    righe.append(f'Firmware: installato {fw}' + (' -> aggiornamento disponibile' if c.firmware_nuovo else ', aggiornato'))
    righe.append(f'App: installata {c.app_installata}' + (' -> aggiornamento disponibile' if c.app_nuova else ', aggiornata'))
    return righe
