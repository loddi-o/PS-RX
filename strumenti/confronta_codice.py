#!/usr/bin/env python3
"""
PS-RX - confronta il codice macchina dei percorsi critici (input del controller, report HID,
audio, invio Bluetooth) fra la build pulita di DS5-Linux-Bridge e quella di PS-RX, e misura l'heap.

    python strumenti/confronta_codice.py riferimento.elf ps-rx.elf
    python strumenti/confronta_codice.py --solo-heap ps-rx.elf

Le istruzioni si confrontano dopo aver tolto indirizzi e codifiche (cambiano solo perche' il
codice si e' spostato). PS-RX cambia apposta alcune funzioni (DualShock 4, polling per controller,
LED del posto...): stanno in MODIFICATE con il motivo. Tutte le altre devono restare identiche;
se una cambia senza essere in elenco, lo script esce con 1.
"""

import difflib
import os
import re
import subprocess
import sys

STRUMENTI = os.path.join(os.path.expanduser('~'), '.ds5-build', 'arm-gnu-toolchain', 'bin')

# Funzioni del percorso per pacchetto (nome C++ senza parametri).
CRITICHE = [
    'on_bt_data', 'interrupt_loop', 'realtime_hid_queue_push', 'realtime_hid_queue_pop',
    'realtime_hid_queue_requeue_front', 'handle_hid_report_failure', 'state_push_slot_to_bt', 'state_push_to_bt',
    'tud_hid_get_report_cb', 'tud_hid_set_report_cb', 'tud_audio_set_itf_cb',
    'audio_loop', 'core1_entry', 'mic_proc', 'speaker_proc', 'mic_add_queue', 'set_headset',
    'bt_pump', 'bt_write', 'bt_send_pending', 'l2cap_packet_handler', 'get_feature_data', 'set_feature_data',
    'state_update', 'state_get', 'wake_on_bt_input', 'usb_variant_task',
]

# Funzioni critiche che PS-RX modifica apposta: nome -> motivo. Si aggiornano insieme al codice.
MODIFICATE = {
    'on_bt_data': "ramo del DualShock 4 (report 0x11) valutato solo quando il report non e' 0x31",
    'bt_write': "un DualShock 4 riceve lo stato tradotto nel suo report 0x11; per il DualSense un controllo in piu'",
    'usb_variant_task': "il bersaglio USB ha anche il mouse del touchpad (un campo in piu' da confrontare e copiare)",
    'state_update':"con 'colore secondo il posto' attivo i colori dell'host non si copiano (un AND in piu')",
    'state_push_slot_to_bt': "stessa funzione, ora anche fuori linea: la chiama posti.cpp per il colore del posto",
    'l2cap_packet_handler':"solo il ramo di apertura del canale: contiene bt_write e init_feature (inline), che "
                            "ora chiede anche il report 0xA3. Nel ramo dei dati cambia solo l'assegnazione dei registri: "
                            "un salvataggio sullo stack attorno alla chiamata (2 istruzioni, ~15 ns a pacchetto)",
}

INDIRIZZO = re.compile(r'\b(?:0x)?[12]00[0-9a-f]{5}\b')


def strumento(nome):
    exe = os.path.join(STRUMENTI, f'arm-none-eabi-{nome}' + ('.exe' if os.name == 'nt' else ''))
    return exe if os.path.exists(exe) else f'arm-none-eabi-{nome}'


def simboli(elf):
    uscita = subprocess.run([strumento('nm'), elf], capture_output=True, text=True, check=True).stdout
    tabella = {}
    for riga in uscita.splitlines():
        parti = riga.split()
        if len(parti) == 3:
            tabella[parti[2]] = int(parti[0], 16)
    return tabella


def heap(elf):
    s = simboli(elf)
    return s['__StackLimit'] - s['__bss_end__']


def funzioni(elf):
    """nome base (senza parametri e senza [clone]) -> istruzioni normalizzate (prima occorrenza).

    -D perche' le funzioni copiate in RAM (.time_critical) finiscono in sezioni non marcate come
    codice, che -d salterebbe."""
    uscita = subprocess.run([strumento('objdump'), '-D', '-C', '-j', '.text', '-j', '.data', elf],
                            capture_output=True, text=True, check=True).stdout
    risultato, attuale, corpo = {}, None, []
    for riga in uscita.splitlines():
        intestazione = re.match(r'^[0-9a-f]{8} <(.+)>:$', riga)
        if intestazione:
            if attuale and attuale not in risultato:
                risultato[attuale] = corpo
            attuale = re.sub(r' \[clone [^\]]+\]', '', intestazione.group(1)).split('(')[0]
            corpo = []
            continue
        if attuale is None:
            continue
        campi = riga.split('\t')
        if len(campi) < 2 or not re.match(r'^\s*[0-9a-f]+:$', campi[0]):
            continue
        grezzo = campi[1].strip()
        if re.fullmatch(r'[0-9a-f]{8}', grezzo):
            v = int(grezzo, 16)
            indirizzo = 0x10000000 <= v < 0x10400000 or 0x20000000 <= v < 0x20082000
            corpo.append('DATO ADDR' if indirizzo else f'DATO {grezzo}')
            continue
        testo = '\t'.join(campi[2:]).split(';')[0].split('@')[0].strip()
        corpo.append(INDIRIZZO.sub('ADDR', testo))
    if attuale and attuale not in risultato:
        risultato[attuale] = corpo
    return risultato


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--solo-heap':
        print(f'[PS-RX] heap libero (regione fra .bss e stack): {heap(sys.argv[2]):,} byte'.replace(',', '.'))
        return 0
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    rif, nuovo = sys.argv[1], sys.argv[2]
    h_rif, h_nuovo = heap(rif), heap(nuovo)
    print(f'[PS-RX] heap: riferimento {h_rif:,} byte, PS-RX {h_nuovo:,} byte, '
          f'differenza {h_nuovo - h_rif:+,}'.replace(',', '.'))
    f_rif, f_nuovo = funzioni(rif), funzioni(nuovo)
    impreviste = 0
    print(f'[PS-RX] {"funzione critica":<34} {"istruzioni":>10}  esito')
    for nome in CRITICHE:
        a, b = f_rif.get(nome), f_nuovo.get(nome)
        if a is None and b is None:
            print(f'        {nome:<34} {"-":>10}  assente in entrambe (inline)')
            continue
        if a == b:
            nota = '  (in MODIFICATE ma identica: togliere?)' if nome in MODIFICATE else ''
            print(f'        {nome:<34} {len(a):>10}  identica{nota}')
            continue
        if nome in MODIFICATE:
            print(f'        {nome:<34} {len(a or []):>10}  modificata apposta ({len(b or [])}): {MODIFICATE[nome]}')
            continue
        impreviste += 1
        print(f'        {nome:<34} {len(a or []):>10}  DIVERSA ({len(b or [])} istruzioni in PS-RX)')
        for riga in list(difflib.unified_diff(a or [], b or [], lineterm='', n=1))[:40]:
            print('            ' + riga)
    if impreviste:
        print(f'[PS-RX] ATTENZIONE: {impreviste} funzioni critiche cambiate senza essere in MODIFICATE')
        return 1
    print('[PS-RX] percorsi critici: identici al riferimento, salvo le modifiche volute')
    return 0


if __name__ == '__main__':
    sys.exit(main())
