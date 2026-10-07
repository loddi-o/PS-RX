"""
PS-RX - descrizione delle impostazioni per le interfacce (app Windows, plugin Decky, pagina web).

Unico posto con etichette, limiti e note: le interfacce costruiscono i controlli da qui.
"""

from __future__ import annotations

from . import protocollo as p

# Impostazioni del ricevitore (sezione Sistema). 'riconnette': cambiarla ricollega l'USB del ricevitore
# (i controller restano collegati, ma il PC li vede sparire e ricomparire per un attimo).
IMPOSTAZIONI = [
    {
        'id': p.IMP_MODALITA, 'chiave': 'modalita', 'tipo': 'scelta', 'sezione': 'sistema', 'riconnette': True,
        'titolo': 'Modalità del ricevitore',
        'opzioni': [(0, 'PlayStation'), (1, 'Xbox'), (2, 'Steam Controller (sperimentale)')],
        'predefinito': 0,
        'nota': 'Come il PC vede i controller. PlayStation: DualSense (anche il DualShock 4), con tutto. Xbox: '
                'controller Xbox 360 (XInput), senza giroscopio e audio; il touchpad funziona solo come mouse. '
                'Steam: mouse e tastiera finché Steam non lo prende.',
    },
    {
        'id': p.IMP_POSTI_FISSI, 'chiave': 'posti_fissi', 'tipo': 'booleano', 'sezione': 'sistema', 'riconnette': True,
        'titolo': 'Sempre 4 gamepad sull\'USB',
        'predefinito': 0,
        'nota': 'Attivo: il PC vede sempre 4 gamepad e collegare un controller non interrompe gli altri. '
                'Spento: il PC vede solo quelli accesi, ma quando se ne collega uno nuovo gli altri si fermano per circa 1 s.',
    },
    {
        'id': p.IMP_LED_POSTO, 'chiave': 'led_posto', 'tipo': 'booleano', 'sezione': 'gamepad', 'riconnette': False,
        'titolo': 'Colore della barra luminosa secondo il posto',
        'predefinito': 0,
        'nota': 'Come la PS5: 1 blu, 2 rosso, 3 verde, 4 rosa. Ignora i colori decisi dai giochi.',
    },
    {
        'id': p.IMP_WOL_SPENTO, 'chiave': 'wol_spento', 'tipo': 'booleano', 'sezione': 'rete', 'riconnette': False,
        'titolo': 'Disattiva il Wake-on-LAN',
        'predefinito': 0,
        'nota': 'Senza Wake-on-LAN il WiFi si spegne appena si collega un controller.',
    },
    {
        'id': p.IMP_BUFFER_AUDIO, 'chiave': 'buffer_audio', 'tipo': 'intero', 'min': 16, 'max': 128, 'passo': 8,
        'sezione': 'sistema', 'riconnette': False,
        'titolo': 'Buffer audio del controller',
        'predefinito': 64,
        'nota': 'Più alto = meno scatti in cuffia, con un po\' di ritardo solo sull\'audio. Non tocca i tasti.',
    },
    {
        'id': p.IMP_INATTIVITA_MIN, 'chiave': 'inattivita', 'tipo': 'intero', 'min': 5, 'max': 60, 'passo': 5,
        'sezione': 'sistema', 'riconnette': False,
        'titolo': 'Spegni il controller dopo (minuti di inattività)',
        'predefinito': 30,
        'nota': 'Il controller si spegne se resta fermo per questo tempo.',
    },
    {
        'id': p.IMP_MAI_DISCONNETTERE, 'chiave': 'mai_disconnettere', 'tipo': 'booleano', 'sezione': 'sistema',
        'riconnette': False,
        'titolo': 'Non spegnere mai i controller per inattività',
        'predefinito': 0,
        'nota': '',
    },
    {
        'id': p.IMP_LED_PICO_SPENTO, 'chiave': 'led_pico_spento', 'tipo': 'booleano', 'sezione': 'sistema',
        'riconnette': False,
        'titolo': 'LED del ricevitore spento',
        'predefinito': 0,
        'nota': 'Di solito il LED del Pico resta acceso finché c\'è almeno un controller collegato.',
    },
    {
        'id': p.IMP_TASTIERA_RISVEGLIO, 'chiave': 'tastiera_risveglio', 'tipo': 'booleano', 'sezione': 'sistema',
        'riconnette': True,
        'titolo': 'Sveglia il PC dalla sospensione via USB',
        'predefinito': 0,
        'nota': 'Aggiunge una piccola tastiera USB che preme un tasto quando premi PS a PC sospeso. Alcuni '
                'anticheat la notano: tienila spenta se non ti serve (il Wake-on-LAN funziona comunque).',
    },
    {
        'id': p.IMP_REGISTRO, 'chiave': 'registro', 'tipo': 'booleano', 'sezione': 'sistema', 'riconnette': False,
        'titolo': 'Registro diagnostico',
        'predefinito': 0,
        'nota': 'Tiene in RAM le ultime righe di log del ricevitore, da leggere nell\'app.',
    },
]

# Impostazioni per controller (sezione Gamepad), salvate per MAC: seguono il controller.
IMPOSTAZIONI_PAD = [
    {
        'id': p.PAD_AUDIO, 'chiave': 'audio', 'tipo': 'booleano', 'predefinito': 1,
        'titolo': 'Audio (altoparlante e cuffie del controller)',
        'nota': 'Solo con un controller collegato: con due o più il Bluetooth non regge l\'audio. Spento libera '
                'banda e rende la connessione più stabile. Non disponibile sul DualShock 4.',
    },
    {
        'id': p.PAD_MICROFONO, 'chiave': 'microfono', 'tipo': 'booleano', 'predefinito': 1,
        'titolo': 'Microfono (integrato o delle cuffie)',
        'nota': 'Trasmette solo quando un\'app usa il microfono.',
    },
    {
        'id': p.PAD_POLLING, 'chiave': 'polling', 'tipo': 'scelta', 'predefinito': 0,
        'opzioni': [(0, '1000 Hz'), (1, '500 Hz'), (2, '250 Hz'), (3, '125 Hz')],
        'titolo': 'Frequenza verso il PC',
        'nota': 'Quante volte al secondo il PC riceve lo stato di questo controller. 1000 Hz = latenza minima.',
    },
    {
        'id': p.PAD_TRACKPAD, 'chiave': 'trackpad', 'tipo': 'booleano', 'predefinito': 0,
        'titolo': 'Touchpad come mouse',
        'nota': 'Il touchpad muove il puntatore e il suo click è il tasto sinistro; la striscia a sinistra '
                'fa da rotellina.',
    },
    {
        'id': p.PAD_INVERTI_SCORRIMENTO, 'chiave': 'inverti_scorrimento', 'tipo': 'booleano', 'predefinito': 0,
        'titolo': 'Inverti la rotellina',
        'nota': '',
    },
]

PER_ID = {v['id']: v for v in IMPOSTAZIONI}
PER_CHIAVE = {v['chiave']: v for v in IMPOSTAZIONI}
PAD_PER_ID = {v['id']: v for v in IMPOSTAZIONI_PAD}
PAD_PER_CHIAVE = {v['chiave']: v for v in IMPOSTAZIONI_PAD}


def _valido(voce: dict, valore: int) -> bool:
    if voce['tipo'] == 'booleano':
        return valore in (0, 1)
    if voce['tipo'] == 'scelta':
        return valore in [o[0] for o in voce['opzioni']]
    return voce['min'] <= valore <= voce['max']


def valido(ident: int, valore: int) -> bool:
    voce = PER_ID.get(ident)
    return voce is not None and _valido(voce, valore)


def valido_pad(ident: int, valore: int) -> bool:
    voce = PAD_PER_ID.get(ident)
    return voce is not None and _valido(voce, valore)


def testo_valore(voce: dict, valore) -> str:
    if valore is None:
        return '?'
    if voce['tipo'] == 'booleano':
        return 'sì' if valore else 'no'
    if voce['tipo'] == 'scelta':
        return dict(voce['opzioni']).get(valore, str(valore))
    return str(valore)
