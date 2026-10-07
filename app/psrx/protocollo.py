"""
PS-RX - protocollo USB (specchio di firmware/src/psrx/protocollo.h; un test controlla i numeri).

Richieste di controllo vendor sull'endpoint 0, destinatario dispositivo, bRequest RICHIESTA,
wValue = comando, wIndex = argomento. Strutture little-endian impacchettate.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from typing import Dict, List, Optional
from .lingua import Testi, tr
from .lingua import tr

RICHIESTA = 0x50
PROTOCOLLO = 1
DATI_MAX = 4096
SETTORE = 4096
PAD_MAX = 4
RETI_MAX = 5
SLOT_TUTTI = 0xFF
STATO_SILENZIOSO = 0x0001
MAGIC = b'PSRX'

# Identita' USB del ricevitore secondo la modalita' (il Pico si riconosce dal magic, non dall'id).
IDENTITA = [
    (0x054C, 0x0CE6),   # PlayStation: DualSense
    (0x054C, 0x0DF2),   # PlayStation: DualSense Edge
    (0x1209, 0x0001),   # Xbox: composito con interfacce XInput (pid.codes, vedi firmware/src/psrx/xbox.h)
    (0x28DE, 0x1304),   # Steam: dongle del nuovo Steam Controller (firmware/src/psrx/steam_usb.h)
]
GUID_INTERFACCIA = '{6F1D2C3B-8A4E-4C59-9B7A-52A1D3E0C7F1}'   # Windows: interfaccia WinUSB di PS-RX

# --- comandi -------------------------------------------------------------------------
CMD_INFO = 0x00
CMD_STATO = 0x01
CMD_IMPOSTAZIONI = 0x02
CMD_ABBINATI = 0x03
CMD_REGISTRO = 0x04
CMD_ERRORE = 0x05
CMD_CARICAMENTO = 0x06
CMD_EVENTI = 0x07
CMD_RETI = 0x08
CMD_PROVA_RETE = 0x09
CMD_RETI_VISTE = 0x0A
CMD_IMPOSTA = 0x10
CMD_SALVA_ORA = 0x11
CMD_PREDEFINITE = 0x12
CMD_IMPOSTA_PAD = 0x13
CMD_RINOMINA = 0x20
CMD_DIMENTICA = 0x21
CMD_DIMENTICA_TUTTI = 0x22
CMD_ABBINA = 0x23
CMD_SPEGNI_PAD = 0x24
CMD_RETE_SALVA = 0x30
CMD_RETE_CANCELLA = 0x31
CMD_WOL_DESTINAZIONI = 0x32
CMD_WOL_PROVA = 0x33
CMD_RETE_PROVA = 0x34
CMD_CERCA_RETI = 0x35
CMD_CARICA_INIZIO = 0x40
CMD_CARICA_BLOCCO = 0x41
CMD_CARICA_FINE = 0x42
CMD_CARICA_ANNULLA = 0x43
CMD_INSTALLA_ORA = 0x44
CMD_BOOTSEL_ORA = 0x45

# --- errori --------------------------------------------------------------------------
ERR_NESSUNO = 0
ERR_COMANDO = 1
ERR_LUNGHEZZA = 2
ERR_VALORE = 3
ERR_OCCUPATO = 4
ERR_PAD_CONNESSO = 5
ERR_NON_TROVATO = 6
ERR_PIENO = 7
ERR_FLASH = 8
ERR_SEQUENZA = 9
ERR_DIMENSIONE = 10
ERR_VERIFICA = 11
ERR_IMMAGINE = 12
ERR_NON_PRONTO = 13
ERR_NON_CONNESSO = 14

TESTO_ERRORE = Testi({
    ERR_NESSUNO: 'nessun errore',
    ERR_COMANDO: 'comando sconosciuto',
    ERR_LUNGHEZZA: 'lunghezza dei dati sbagliata',
    ERR_VALORE: 'valore non ammesso',
    ERR_OCCUPATO: "il ricevitore sta ancora finendo l'operazione precedente",
    ERR_PAD_CONNESSO: 'spegni prima i controller (PS-RX scrive in memoria solo senza controller collegati)',
    ERR_NON_TROVATO: 'controller non trovato fra gli abbinati',
    ERR_PIENO: 'posti esauriti',
    ERR_FLASH: 'scrittura in memoria fallita',
    ERR_SEQUENZA: 'blocco fuori ordine o caricamento non iniziato',
    ERR_DIMENSIONE: 'firmware di dimensione non valida',
    ERR_VERIFICA: 'impronta SHA-256 diversa: firmware rovinato durante il caricamento',
    ERR_IMMAGINE: "il file non e' un firmware per Raspberry Pi Pico 2 W",
    ERR_NON_PRONTO: 'nessun firmware pronto da installare',
    ERR_NON_CONNESSO: 'WiFi non connesso',
})

# --- impostazioni globali ----------------------------------------------------------------
IMP_MODALITA = 1
IMP_POSTI_FISSI = 2
IMP_LED_POSTO = 3
IMP_WOL_SPENTO = 4
IMP_BUFFER_AUDIO = 5
IMP_INATTIVITA_MIN = 6
IMP_MAI_DISCONNETTERE = 7
IMP_LED_PICO_SPENTO = 8
IMP_TASTIERA_RISVEGLIO = 9
IMP_REGISTRO = 10
IMP_SPEGNI_CON_PC = 11
N_IMPOSTAZIONI = 11

# --- impostazioni per controller ------------------------------------------------------------
PAD_AUDIO = 1
PAD_MICROFONO = 2
PAD_POLLING = 3
PAD_TRACKPAD = 4
PAD_INVERTI_SCORRIMENTO = 5

MODALITA = Testi({0: 'PlayStation', 1: 'Xbox', 2: 'Steam Controller'})
POLLING_HZ = {0: 1000, 1: 500, 2: 250, 3: 125}

# --- modelli -------------------------------------------------------------------------------
MODELLO_NESSUNO = 0
MODELLO_DUALSENSE = 1
MODELLO_EDGE = 2
MODELLO_DUALSHOCK4 = 3
MODELLO_DS4 = 3
NOME_MODELLO = Testi({0: '-', 1: 'DualSense', 2: 'DualSense Edge', 3: 'DualShock 4'})

# --- caricamento ------------------------------------------------------------------------------
CAR_INATTIVO = 0
CAR_RICEZIONE = 1
CAR_SCRITTURA = 2
CAR_VERIFICA = 3
CAR_PRONTO = 4
CAR_INSTALLAZIONE = 5
CAR_ERRORE = 6

# --- eventi ------------------------------------------------------------------------------------
EVENTO_COLLEGATO = 1
EVENTO_SCOLLEGATO = 2
EVENTO_BATTERIA = 3

# --- rete --------------------------------------------------------------------------------------
RETE_SPENTA = 0
RETE_CONNESSIONE = 1
RETE_CONNESSA = 2
RETE_ERRORE = 3
TESTO_RETE = Testi({0: 'spento (radio al Bluetooth)', 1: 'connessione in corso', 2: 'connesso', 3: 'nessuna rete raggiungibile'})

CAP_AGGIORNAMENTO_APP = 1 << 0
CAP_WOL = 1 << 1
CAP_REGISTRO = 1 << 2
CAP_DS4 = 1 << 3
CAP_XBOX = 1 << 4
CAP_STEAM = 1 << 5
CAP_TRACKPAD = 1 << 6

# --- strutture -----------------------------------------------------------------------------------
FMT_INFO = '<4sBBHII32s28s'
FMT_PAD = '<BBBBb6sBH6s'
FMT_STATO = '<BBBBBBBBIIHBb4shBBI'
FMT_VOCE = '<BH'
FMT_ABBINATO = '<6s16sBBBBBBB'
FMT_IMPOSTA_PAD = '<6sBB'
FMT_EVENTO = '<HBBBBBB6s'
FMT_VOCE_RETE = '<33sBB'
FMT_RETE_DATI = '<33s64sBB'
FMT_CARICAMENTO = '<BBHHHI'
FMT_CARICA_INIZIO = '<I32s'
FMT_ERRORE = '<BB'
FMT_PROVA_RETE = '<BBbb4s4sHH'   # ProvaRete (firmware/src/psrx/rete.h)
FMT_RETE_VISTA = '<33sbBB'        # ReteVista (rete.h)
RETI_VISTE_MAX = 16
DIM_RETI_VISTE = 2 + RETI_VISTE_MAX * struct.calcsize(FMT_RETE_VISTA)
SCAN_NESSUNA, SCAN_IN_CORSO, SCAN_FINITA, SCAN_ANNULLATA = range(4)

# Prova del WiFi: fasi ed esiti (rete.h)
PROVA_NESSUNA, PROVA_CONNESSIONE, PROVA_INDIRIZZO, PROVA_INTERNET, PROVA_FINITA = range(5)
ESITO_IN_CORSO, ESITO_OK, ESITO_PASSWORD, ESITO_NON_TROVATA, ESITO_NESSUNA_RISPOSTA, ESITO_NESSUN_IP, \
    ESITO_NO_INTERNET, ESITO_ANNULLATA = range(8)
TESTO_FASE_PROVA = Testi({PROVA_CONNESSIONE: 'collegamento alla rete', PROVA_INDIRIZZO: 'indirizzo IP dal router',
                    PROVA_INTERNET: 'verifica di internet'})
TESTO_ESITO_PROVA = Testi({
    ESITO_OK: 'tutto a posto: password giusta, indirizzo dal router e internet raggiungibile',
    ESITO_PASSWORD: 'password sbagliata',
    ESITO_NON_TROVATA: 'rete non trovata: è spenta, troppo lontana, il nome è sbagliato o trasmette solo a 5 GHz (il Pico 2 W usa solo i 2,4 GHz)',
    ESITO_NESSUNA_RISPOSTA: 'il router non ha completato il collegamento (riprova; controlla WPA2/WPA3)',
    ESITO_NESSUN_IP: 'collegato, ma il router non ha dato un indirizzo IP (DHCP spento o pieno?)',
    ESITO_NO_INTERNET: 'rete di casa raggiungibile, internet no (il Wake-on-LAN funziona lo stesso: resta in casa)',
    ESITO_ANNULLATA: 'prova interrotta: si è collegato un controller e il WiFi si è spento',
})

DIM_INFO = struct.calcsize(FMT_INFO)
DIM_PAD = struct.calcsize(FMT_PAD)
DIM_STATO = struct.calcsize(FMT_STATO) + PAD_MAX * DIM_PAD
DIM_ABBINATO = struct.calcsize(FMT_ABBINATO)
DIM_EVENTO = struct.calcsize(FMT_EVENTO)
DIM_VOCE_RETE = struct.calcsize(FMT_VOCE_RETE)
DIM_RETI = RETI_MAX * DIM_VOCE_RETE + 13
assert (DIM_INFO, DIM_PAD, DIM_STATO, DIM_ABBINATO, DIM_EVENTO) == (76, 20, 112, 29, 14)


def mac_testo(b: bytes) -> str:
    return ':'.join(f'{x:02X}' for x in b)


def mac_da_testo(t: str) -> bytes:
    parti = t.replace('-', ':').split(':')
    if len(parti) != 6:
        raise ValueError(tr('MAC non valido: {0}', t))
    return bytes(int(x, 16) for x in parti)


def _stringa(b: bytes) -> str:
    return b.split(b'\0', 1)[0].decode('utf-8', 'replace')


@dataclass
class Info:
    protocollo: int
    slot_max: int
    capacita: int
    staging_max: int
    versione: str
    base: str


@dataclass
class Pad:
    posto: int
    connesso: bool
    modello: int
    batteria: int
    batteria_valida: bool
    in_carica: bool
    audio: bool
    rssi: Optional[int]
    mac: str
    polling: int
    report_al_secondo: int

    @property
    def nome_modello(self) -> str:
        return NOME_MODELLO.get(self.modello, '?')


@dataclass
class Stato:
    modalita: int
    usb_gamepad: int
    usb_configurato: bool
    usb_sospeso: bool
    tastiera_risveglio: bool
    altoparlante: bool
    microfono: bool
    microfono_muto: bool
    pad_connessi: int
    finestra_abbinamento: bool
    salvataggio_in_sospeso: bool
    posti_fissi: bool
    caricamento: int
    uptime_s: int
    heap_libero: int
    ultimo_evento: int
    rete_stato: int
    rete_indice: int
    rete_ip: str
    rete_rssi: int
    wol_finestra: bool
    ms_da_ultimo_wol: Optional[int]
    pad: List[Pad] = field(default_factory=list)

    @property
    def nome_modalita(self) -> str:
        return MODALITA.get(self.modalita, '?')


@dataclass
class Abbinato:
    mac: str
    nome: str
    posto: Optional[int]
    audio: bool
    microfono: bool
    polling: int
    trackpad: bool
    inverti_scorrimento: bool


@dataclass
class Evento:
    numero: int
    tipo: int
    posto: int
    modello: int
    batteria: Optional[int]
    critico: bool
    modalita: int
    mac: str


@dataclass
class Rete:
    indice: int
    ssid: str
    wpa3: bool
    ha_password: bool


@dataclass
class Reti:
    reti: List[Rete]
    wol_mac: List[str]        # '' = nessuna
    wol_spento: bool


@dataclass
class Caricamento:
    stato: int
    errore: int
    scritti: int
    totali: int
    dimensione: int

    @property
    def descrizione(self) -> str:
        testi = {CAR_INATTIVO: tr('nessun firmware in caricamento'),
                 CAR_RICEZIONE: tr('ricezione: {0}/{1} blocchi', self.scritti, self.totali),
                 CAR_SCRITTURA: tr('scrittura: {0}/{1} blocchi', self.scritti, self.totali),
                 CAR_VERIFICA: tr("verifica dell'impronta SHA-256"),
                 CAR_PRONTO: tr('firmware verificato, pronto da installare'),
                 CAR_INSTALLAZIONE: tr('installazione in corso (non staccare il ricevitore)'),
                 CAR_ERRORE: tr('errore: {0}', TESTO_ERRORE.get(self.errore, self.errore))}
        return testi.get(self.stato, '?')


@dataclass
class ProvaRete:
    fase: int
    esito: int
    rete: int
    rssi: Optional[int]
    ip: str
    gateway: str
    ms_internet: int
    ms_totale: int

    @property
    def finita(self) -> bool:
        return self.fase == PROVA_FINITA

    @property
    def descrizione(self) -> str:
        if self.fase == PROVA_NESSUNA:
            return tr('nessuna prova')
        if not self.finita:
            return tr('in corso: {0}…', TESTO_FASE_PROVA.get(self.fase, '?'))
        testo = TESTO_ESITO_PROVA.get(self.esito, tr('esito {0}', self.esito))
        if self.esito == ESITO_OK:
            testo += tr(' (IP {0}, segnale {1} dBm, internet in {2} ms)', self.ip, self.rssi, self.ms_internet)
        return testo


@dataclass
class ReteVista:
    ssid: str
    rssi: int
    canale: int
    sicurezza: int      # 0 = aperta

    @property
    def aperta(self) -> bool:
        return self.sicurezza == 0

    @property
    def tacche(self) -> int:
        """Segnale da 1 a 4."""
        return 4 if self.rssi >= -55 else 3 if self.rssi >= -67 else 2 if self.rssi >= -75 else 1


@dataclass
class RetiViste:
    stato: int
    reti: List[ReteVista]

    @property
    def finita(self) -> bool:
        return self.stato in (SCAN_FINITA, SCAN_ANNULLATA)


def leggi_reti_viste(b: bytes) -> RetiViste:
    stato, n = b[0], b[1]
    dim = struct.calcsize(FMT_RETE_VISTA)
    reti = []
    for i in range(min(n, RETI_VISTE_MAX)):
        ssid, rssi, canale, sic = struct.unpack_from(FMT_RETE_VISTA, b, 2 + i * dim)
        reti.append(ReteVista(_stringa(ssid), rssi, canale, sic))
    return RetiViste(stato, reti)


def leggi_prova_rete(b: bytes) -> ProvaRete:
    fase, esito, rete, rssi, ip, gw, ms_i, ms_t = struct.unpack_from(FMT_PROVA_RETE, b)
    return ProvaRete(fase, esito, rete, rssi or None, '.'.join(str(x) for x in ip), '.'.join(str(x) for x in gw),
                     ms_i, ms_t)


def leggi_info(b: bytes) -> Info:
    magic, protocollo, slot_max, _, capacita, staging, versione, base = struct.unpack_from(FMT_INFO, b)
    if magic != MAGIC:
        raise ValueError(tr("non e' un PS-RX"))
    return Info(protocollo, slot_max, capacita, staging, _stringa(versione), _stringa(base))


def leggi_stato(b: bytes) -> Stato:
    v = struct.unpack_from(FMT_STATO, b)
    (_, modalita, usb_gamepad, usb_flag, audio_flag, pad_connessi, flag, caricamento, uptime, heap, ultimo,
     rete_stato, rete_indice, ip, rssi, wol_finestra, _, ms_wol) = v
    s = Stato(modalita, usb_gamepad, bool(usb_flag & 1), bool(usb_flag & 2), bool(usb_flag & 4),
              bool(audio_flag & 1), bool(audio_flag & 2), bool(audio_flag & 4), pad_connessi,
              bool(flag & 1), bool(flag & 2), bool(flag & 4), caricamento, uptime, heap, ultimo,
              rete_stato, rete_indice, '.'.join(str(x) for x in ip), rssi, bool(wol_finestra),
              None if ms_wol == 0xFFFFFFFF else ms_wol)
    base = struct.calcsize(FMT_STATO)
    for i in range(PAD_MAX):
        (connesso, modello, batteria, pflag, prssi, mac, polling, rps, _) = struct.unpack_from(FMT_PAD, b, base + i * DIM_PAD)
        s.pad.append(Pad(i, bool(connesso), modello, batteria, bool(pflag & 1), bool(pflag & 2), bool(pflag & 4),
                         None if prssi == 127 else prssi, mac_testo(mac), polling, rps))
    return s


def leggi_impostazioni(b: bytes) -> Dict[int, int]:
    n = b[0]
    return {i: v for i, v in (struct.unpack_from(FMT_VOCE, b, 1 + k * 3) for k in range(n))}


def leggi_abbinati(b: bytes) -> List[Abbinato]:
    risultato = []
    for k in range(b[0]):
        mac, nome, posto, audio, micro, polling, trackpad, inverti, _ = struct.unpack_from(FMT_ABBINATO, b, 1 + k * DIM_ABBINATO)
        risultato.append(Abbinato(mac_testo(mac), _stringa(nome), None if posto == 0xFF else posto, bool(audio),
                                  bool(micro), polling, bool(trackpad), bool(inverti)))
    return risultato


def leggi_eventi(b: bytes) -> List[Evento]:
    risultato = []
    for k in range(b[0]):
        numero, tipo, posto, modello, batteria, flag, modalita, mac = struct.unpack_from(FMT_EVENTO, b, 1 + k * DIM_EVENTO)
        risultato.append(Evento(numero, tipo, posto, modello, None if batteria == 0xFF else batteria, bool(flag),
                                modalita, mac_testo(mac)))
    return risultato


def leggi_reti(b: bytes) -> Reti:
    reti = []
    for i in range(RETI_MAX):
        ssid, auth, ha_pw = struct.unpack_from(FMT_VOCE_RETE, b, i * DIM_VOCE_RETE)
        reti.append(Rete(i, _stringa(ssid), bool(auth), bool(ha_pw)))
    o = RETI_MAX * DIM_VOCE_RETE
    macs = [b[o:o + 6], b[o + 6:o + 12]]
    return Reti(reti, ['' if not any(m) else mac_testo(m) for m in macs], bool(b[o + 12]))


def leggi_caricamento(b: bytes) -> Caricamento:
    stato, errore, scritti, totali, _, dim = struct.unpack_from(FMT_CARICAMENTO, b)
    return Caricamento(stato, errore, scritti, totali, dim)


def testo_evento(e: Evento, nome: str = '') -> tuple:
    """(titolo, testo) della notifica per un evento."""
    chi = nome or tr('Controller {0}', e.posto + 1)
    batteria = tr('batteria sconosciuta') if e.batteria is None else tr('batteria {0}%', e.batteria)
    if e.tipo == EVENTO_COLLEGATO:
        return (tr('{0} collegato', chi),
                tr('Posto {0} · {1} · modalità {2} · {3}', e.posto + 1, NOME_MODELLO.get(e.modello, '?'),
                   MODALITA.get(e.modalita, '?'), batteria))
    if e.tipo == EVENTO_SCOLLEGATO:
        return (tr('{0} scollegato', chi), tr('Posto {0}', e.posto + 1))
    if e.tipo == EVENTO_BATTERIA:
        if e.critico:
            return (tr('Batteria quasi scarica'), tr('{0}: {1}, collega il cavo', chi, batteria))
        return (tr('Batteria in esaurimento'), tr('{0}: {1}', chi, batteria))
    return ('PS-RX', tr('evento {0}', e.tipo))
