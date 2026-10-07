"""
PS-RX - riga di comando.

    python -m psrx stato
    python -m psrx impostazioni | imposta CHIAVE VALORE
    python -m psrx abbinati | pad MAC CHIAVE VALORE | rinomina MAC NOME | dimentica MAC | abbina | spegni [POSTO]
    python -m psrx reti | rete POSTO SSID [PASSWORD] [--wpa3] | rete-cancella POSTO | rete-prova POSTO | reti-vicine
    python -m psrx wol MAC1 [MAC2] | wol-prova
    python -m psrx eventi            (resta in ascolto e stampa le notifiche)
    python -m psrx registro
    python -m psrx carica firmware.uf2 | bootsel
    python -m psrx aggiornamenti | aggiorna-firmware   (release su GitHub)
    python -m psrx installa-pico [firmware.uf2]        (Pico 2 W nuovo o in BOOTSEL)
    python -m psrx regola-udev       (Linux: regola udev per l'accesso senza root)

Con --simulatore usa un ricevitore finto (per provare senza hardware).
"""

from __future__ import annotations

import argparse
import os
import sys
import time

from . import protocollo as p
from . import schema
from .errori import ErrorePsrx, PermessoNegato, Scollegato
from .firmware import FirmwareNonValido, leggi as leggi_firmware
from .servizio import Interrotto, Psrx
from .lingua import tr


def _durata(s: int) -> str:
    return f'{s // 3600} h {s % 3600 // 60} min' if s >= 3600 else f'{s // 60} min' if s >= 60 else f'{s} s'


def stampa_stato(ps: Psrx) -> None:
    info = ps.info()
    st = ps.stato()
    print(tr('PS-RX {0} (protocollo {1}, base {2}), acceso da {3}', info.versione, info.protocollo, info.base, _durata(st.uptime_s)))
    print(tr('Modalità: {0}; USB: {1} gamepad esposti{2}{3}', st.nome_modalita, st.usb_gamepad, ' (sospeso)' if st.usb_sospeso else '', tr(', posti fissi') if st.posti_fissi else ''))
    rete = p.TESTO_RETE.get(st.rete_stato, '?')
    if st.rete_stato == p.RETE_CONNESSA:
        rete += tr(', IP {0}, segnale {1} dBm', st.rete_ip, st.rete_rssi)
    wol = tr('mai') if st.ms_da_ultimo_wol is None else tr('{0} fa', _durata(st.ms_da_ultimo_wol // 1000))
    print(tr('WiFi: {0}; ultimo Wake-on-LAN: {1}', rete, wol))
    audio = [tr(n) for n, a in (('altoparlante', st.altoparlante), ('microfono', st.microfono)) if a]
    print(tr('Audio: {0}{1}', ', '.join(audio) or tr('non in uso'), tr(' (microfono muto)') if st.microfono_muto else ''))
    if st.finestra_abbinamento:
        print(tr('Finestra di abbinamento aperta'))
    if st.salvataggio_in_sospeso:
        print(tr('Impostazioni in attesa di essere salvate (succede senza controller collegati)'))
    print(tr('Memoria libera: {0} KB', st.heap_libero // 1024) if st.heap_libero else tr('Memoria libera: -'))
    print('Controller:')
    nomi = {a.mac: a.nome for a in ps.abbinati()}
    for pad in st.pad:
        if not pad.connesso:
            continue
        batt = f'{pad.batteria}%{" in carica" if pad.in_carica else ""}' if pad.batteria_valida else '?'
        segnale = '-' if pad.rssi is None else str(pad.rssi)
        print(tr('  posto {0}: {1} {2} {3} batteria {4}, segnale {5}, {6} report/s, polling {7} Hz', pad.posto + 1, pad.nome_modello, pad.mac, nomi.get(pad.mac) or '', batt, segnale, pad.report_al_secondo, p.POLLING_HZ[pad.polling]))


def comando_carica(ps: Psrx, percorso: str) -> None:
    img = leggi_firmware(percorso, ps.info().staging_max)
    print(tr('{0}: {1} byte, versione {2}', img.nome, img.dimensione, img.versione or '?'))

    def avanzamento(fase: str, fatto: int, totale: int) -> None:
        testo = {'attesa_pad': tr('spegni i controller per continuare'),
                 'caricamento': tr('caricamento {0}/{1}', fatto, totale),
                 'verifica': 'verifica', 'installazione': tr('installazione: il ricevitore si riavvia')}[fase]
        print(f'\r{testo:<60}', end='', flush=True)

    ps.carica_firmware(img, avanzamento)
    print(tr('\nfatto: il ricevitore si riavvia col firmware nuovo (circa 15 s, non staccarlo)'))


def comando_aggiornamenti(ps: Psrx, installa: bool) -> int:
    from . import aggiornamenti as ag
    try:
        installato = ps.info().versione
    except Scollegato:
        installato = None
    try:
        c = ag.controlla(installato)
    except ag.ErroreAggiornamento as e:
        print(tr('ricerca non riuscita: {0}', e))
        return 1
    for riga in ag.riepilogo(c):
        print(riga)
    print(c.release.pagina)
    if not installa:
        return 0
    f = ag.file_firmware(c.release)
    if f is None:
        print(tr('la release non contiene il firmware'))
        return 1
    if not c.firmware_nuovo:
        print(tr('il firmware installato è già il più recente'))
        return 0
    import tempfile
    percorso = ag.scarica(f, os.path.join(tempfile.gettempdir(), 'ps-rx'),
                          lambda fatti, totali: print(tr('\rdownload {0}%', fatti * 100 // max(totali, 1)), end='', flush=True))
    print()
    comando_carica(ps, percorso)
    return 0


def comando_installa_pico(file: str) -> int:
    from . import aggiornamenti as ag
    from . import pico_nuovo
    import tempfile
    print(tr("collega il Pico 2 W (se e' gia' stato usato, tieni premuto BOOTSEL mentre lo colleghi)..."))
    fine = time.monotonic() + 120
    while not pico_nuovo.presente():
        if time.monotonic() > fine:
            print(tr("nessun Pico in modalita' BOOTSEL"))
            return 1
        time.sleep(0.5)
    if not file:
        rel = ag.ultima_release()
        f = rel.file.get('firmware')
        if f is None:
            print(tr("l'ultima release non contiene il firmware .uf2"))
            return 1
        file = ag.scarica(f, os.path.join(tempfile.gettempdir(), 'ps-rx'))
    img = leggi_firmware(file)
    print(tr('copia di {0} (versione {1}) nel Pico...', img.nome, img.versione or '?'))
    unita = pico_nuovo.trova()
    if unita:
        pico_nuovo.scrivi_uf2(unita[0], file)
    else:
        pico_nuovo.scrivi_uf2_come_root(file)   # Linux: chiavetta non montata (serve root)
    print(tr('attendo il ricevitore PS-RX...'))
    t = pico_nuovo.attendi_psrx(_apri_usb)
    t.chiudi()
    print(tr("fatto: il ricevitore e' pronto (python -m psrx abbina per il primo controller)"))
    return 0


def _apri_usb():
    from .servizio import apri_usb
    return apri_usb()


def main(argv=None) -> int:
    from . import lingua
    lingua.imposta(lingua.di_sistema())
    a = argparse.ArgumentParser(prog='python -m psrx', description=tr('Gestione del ricevitore PS-RX via USB'))
    a.add_argument('--simulatore', action='store_true', help=tr('usa un ricevitore simulato'))
    sub = a.add_subparsers(dest='comando', required=True)
    sub.add_parser('stato')
    sub.add_parser('impostazioni')
    s = sub.add_parser('imposta')
    s.add_argument('chiave')
    s.add_argument('valore', type=int)
    sub.add_parser('abbinati')
    s = sub.add_parser('pad', help=tr('impostazione per controller'))
    s.add_argument('mac')
    s.add_argument('chiave', choices=sorted(schema.PAD_PER_CHIAVE))
    s.add_argument('valore', type=int)
    s = sub.add_parser('rinomina')
    s.add_argument('mac')
    s.add_argument('nome')
    s = sub.add_parser('dimentica')
    s.add_argument('mac')
    sub.add_parser('dimentica-tutti')
    sub.add_parser('abbina')
    s = sub.add_parser('spegni')
    s.add_argument('posto', type=int, nargs='?', help='1-4; senza = tutti')
    sub.add_parser('reti')
    s = sub.add_parser('rete')
    s.add_argument('posto', type=int, help='1-5')
    s.add_argument('ssid')
    s.add_argument('password', nargs='?', default='')
    s.add_argument('--wpa3', action='store_true')
    s = sub.add_parser('rete-cancella')
    s.add_argument('posto', type=int)
    sub.add_parser('reti-vicine', help=tr('cerca le reti WiFi visibili dal ricevitore (solo 2,4 GHz)'))
    s = sub.add_parser('rete-prova', help=tr('prova una rete salvata (password, indirizzo, internet)'))
    s.add_argument('posto', type=int, help='1-5')
    s = sub.add_parser('wol')
    s.add_argument('mac1', nargs='?', default='')
    s.add_argument('mac2', nargs='?', default='')
    sub.add_parser('wol-prova')
    sub.add_parser('eventi')
    sub.add_parser('registro')
    s = sub.add_parser('carica')
    s.add_argument('file')
    sub.add_parser('bootsel')
    sub.add_parser('regola-udev')
    sub.add_parser('aggiornamenti', help=tr('cerca una nuova versione su GitHub'))
    sub.add_parser('aggiorna-firmware', help=tr("scarica da GitHub e installa l'ultimo firmware"))
    s = sub.add_parser('installa-pico', help=tr('firmware su un Pico 2 W nuovo o in BOOTSEL'))
    s.add_argument('file', nargs='?', default='', help=tr("firmware .uf2 (senza: l'ultimo da GitHub)"))
    args = a.parse_args(argv)

    if args.comando == 'installa-pico':
        try:
            return comando_installa_pico(args.file)
        except (OSError, TimeoutError, FirmwareNonValido) as e:
            print(tr('installazione non riuscita: {0}', e))
            return 1
    if args.comando == 'regola-udev':
        from .permessi import REGOLA
        sys.stdout.write(REGOLA)
        return 0
    if args.simulatore:
        from .simulatore import PicoSimulato
        pico = PicoSimulato()
        ps = Psrx(lambda: pico.trasporto)
    else:
        ps = Psrx()
    try:
        c = args.comando
        if c == 'stato':
            stampa_stato(ps)
        elif c == 'impostazioni':
            valori = ps.impostazioni()
            for voce in schema.IMPOSTAZIONI:
                print(f'{voce["chiave"]:<20} {schema.testo_valore(voce, valori.get(voce["id"])):<34} {voce["titolo"]}')
        elif c == 'imposta':
            voce = schema.PER_CHIAVE.get(args.chiave)
            if voce is None:
                print(tr('impostazione sconosciuta: {0} (vedi "impostazioni")', args.chiave))
                return 2
            ps.imposta(voce['id'], args.valore)
            print(f'{voce["titolo"]} = {schema.testo_valore(voce, args.valore)}')
        elif c == 'abbinati':
            for ab in ps.abbinati():
                stato = tr('collegato (posto {0})', ab.posto + 1) if ab.posto is not None else tr('non collegato')
                print(tr('{0}  {1:<16} {2}; audio {3}, microfono {4}, {5} Hz, touchpad come mouse {6}', ab.mac, ab.nome or tr('(senza nome)'), stato, tr('sì') if ab.audio else 'no', tr('sì') if ab.microfono else 'no', p.POLLING_HZ[ab.polling], tr('sì') if ab.trackpad else 'no'))
        elif c == 'pad':
            voce = schema.PAD_PER_CHIAVE[args.chiave]
            ps.imposta_pad(args.mac, voce['id'], args.valore)
        elif c == 'rinomina':
            ps.rinomina(args.mac, args.nome)
        elif c == 'dimentica':
            ps.dimentica(args.mac)
        elif c == 'dimentica-tutti':
            ps.dimentica_tutti()
        elif c == 'abbina':
            ps.abbina()
            print(tr('finestra di abbinamento di 30 s: tieni Create + PS (DualSense) o Share + PS (DualShock 4)'))
        elif c == 'spegni':
            ps.spegni_pad(None if args.posto is None else args.posto - 1)
        elif c == 'reti':
            r = ps.reti()
            for rete in r.reti:
                if rete.ssid:
                    print(f'{rete.indice + 1}: {rete.ssid} ({"WPA3" if rete.wpa3 else "WPA2"}'
                          f'{", aperta" if not rete.ha_password else ""})')
            print(tr('Wake-on-LAN: {0}{1}', ', '.join((m for m in r.wol_mac if m)) or tr('nessuna destinazione'), tr(' (disattivato)') if r.wol_spento else ''))
        elif c == 'rete':
            ps.salva_rete(args.posto - 1, args.ssid, args.password, args.wpa3)
        elif c == 'rete-cancella':
            ps.cancella_rete(args.posto - 1)
        elif c == 'reti-vicine':
            ps.cerca_reti()
            fine = time.monotonic() + 20
            while True:
                viste = ps.reti_viste()
                if viste.finita or time.monotonic() > fine:
                    break
                time.sleep(0.5)
            if not viste.reti:
                print(tr('nessuna rete rilevata (il ricevitore vede solo le reti a 2,4 GHz)'))
            for r in viste.reti:
                print(tr('{0:<33} {1:>4} dBm  canale {2:>2}  {3}', r.ssid, r.rssi, r.canale, tr('aperta') if r.aperta else tr('protetta')))
        elif c == 'rete-prova':
            ps.prova_rete(args.posto - 1)
            while True:
                esito = ps.esito_prova_rete()
                print(f'\r{esito.descrizione:<100}', end='', flush=True)
                if esito.finita:
                    print()
                    return 0 if esito.esito == p.ESITO_OK else 1
                time.sleep(0.5)
        elif c == 'wol':
            ps.destinazioni_wol(args.mac1, args.mac2)
        elif c == 'wol-prova':
            ps.prova_wol()
            print(tr('pacchetti magici inviati'))
        elif c == 'eventi':
            from .notifiche import Sorvegliante
            sorvegliante = Sorvegliante(ps)
            print(tr('in ascolto (Ctrl+C per uscire)'))
            while True:
                for titolo, testo, critica in sorvegliante.controlla():
                    print(f'{"!! " if critica else ""}{titolo}: {testo}')
                time.sleep(2)
        elif c == 'registro':
            attivo, testo = ps.registro()
            print(testo if attivo else tr('registro spento: python -m psrx imposta registro 1'))
        elif c == 'carica':
            comando_carica(ps, args.file)
        elif c == 'bootsel':
            ps.bootsel_ora()
        elif c in ('aggiornamenti', 'aggiorna-firmware'):
            return comando_aggiornamenti(ps, c == 'aggiorna-firmware')
        return 0
    except PermessoNegato as e:
        print(tr('manca il permesso sul nodo USB {0}: python -m psrx regola-udev | sudo tee {1}/etc/udev/rules.d/70-ps-rx.rules', e, ''))
        return 1
    except Scollegato as e:
        print(tr('ricevitore PS-RX non trovato ({0}). È collegato via USB?', e))
        return 1
    except ErrorePsrx as e:
        print(tr('il ricevitore ha rifiutato: {0}', e))
        return 1
    except FirmwareNonValido as e:
        print(tr('firmware non valido: {0}', e))
        return 1
    except (Interrotto, KeyboardInterrupt):
        print()
        return 130
    finally:
        ps.chiudi()
