"""
PS-RX - trasporto USB su Windows: WinUSB con ctypes, solo libreria standard.

Il firmware dichiara con i descrittori MS OS 2.0 che la sua interfaccia di configurazione usa
WinUSB, con un GUID proprio: Windows aggancia il driver da solo al primo collegamento, senza
installazioni. Qui si cerca l'interfaccia per GUID (SetupAPI), la si apre e si mandano richieste di
controllo vendor con WinUsb_ControlTransfer (destinatario dispositivo, come su Linux).
"""

from __future__ import annotations

import ctypes
import ctypes.wintypes as wt
import uuid
from typing import List

from . import protocollo as p
from .errori import Scollegato, Stallo

DIGCF_PRESENT = 0x02
DIGCF_DEVICEINTERFACE = 0x10
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
FILE_SHARE_READ = 0x01
FILE_SHARE_WRITE = 0x02
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x80
FILE_FLAG_OVERLAPPED = 0x40000000
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
PIPE_TRANSFER_TIMEOUT = 0x03
ERROR_GEN_FAILURE = 31          # STALL
ERRORI_SCOLLEGATO = (2, 21, 22, 433, 1167, 995)


class GUID(ctypes.Structure):
    _fields_ = [('Data1', wt.DWORD), ('Data2', wt.WORD), ('Data3', wt.WORD), ('Data4', ctypes.c_ubyte * 8)]

    @classmethod
    def da_testo(cls, testo: str) -> 'GUID':
        u = uuid.UUID(testo)
        g = cls()
        g.Data1, g.Data2, g.Data3 = u.time_low, u.time_mid, u.time_hi_version
        for i, b in enumerate(u.bytes[8:]):
            g.Data4[i] = b
        return g


class SP_DEVICE_INTERFACE_DATA(ctypes.Structure):
    _fields_ = [('cbSize', wt.DWORD), ('InterfaceClassGuid', GUID), ('Flags', wt.DWORD),
                ('Reserved', ctypes.c_void_p)]


class WINUSB_SETUP_PACKET(ctypes.Structure):
    _pack_ = 1
    _fields_ = [('RequestType', ctypes.c_ubyte), ('Request', ctypes.c_ubyte), ('Value', ctypes.c_ushort),
                ('Index', ctypes.c_ushort), ('Length', ctypes.c_ushort)]


def _api():
    setupapi = ctypes.WinDLL('setupapi', use_last_error=True)
    winusb = ctypes.WinDLL('winusb', use_last_error=True)
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    setupapi.SetupDiGetClassDevsW.restype = ctypes.c_void_p
    setupapi.SetupDiGetClassDevsW.argtypes = [ctypes.POINTER(GUID), wt.LPCWSTR, wt.HWND, wt.DWORD]
    setupapi.SetupDiEnumDeviceInterfaces.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(GUID),
                                                     wt.DWORD, ctypes.POINTER(SP_DEVICE_INTERFACE_DATA)]
    setupapi.SetupDiGetDeviceInterfaceDetailW.argtypes = [ctypes.c_void_p, ctypes.POINTER(SP_DEVICE_INTERFACE_DATA),
                                                          ctypes.c_void_p, wt.DWORD, ctypes.POINTER(wt.DWORD),
                                                          ctypes.c_void_p]
    setupapi.SetupDiDestroyDeviceInfoList.argtypes = [ctypes.c_void_p]
    kernel32.CreateFileW.restype = ctypes.c_void_p
    kernel32.CreateFileW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, ctypes.c_void_p, wt.DWORD, wt.DWORD,
                                     ctypes.c_void_p]
    kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
    winusb.WinUsb_Initialize.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
    winusb.WinUsb_Free.argtypes = [ctypes.c_void_p]
    winusb.WinUsb_ControlTransfer.argtypes = [ctypes.c_void_p, WINUSB_SETUP_PACKET, ctypes.c_void_p, wt.ULONG,
                                              ctypes.POINTER(wt.ULONG), ctypes.c_void_p]
    winusb.WinUsb_SetPipePolicy.argtypes = [ctypes.c_void_p, ctypes.c_ubyte, wt.ULONG, wt.ULONG, ctypes.c_void_p]
    return setupapi, winusb, kernel32


def percorsi_interfaccia(guid_testo: str = p.GUID_INTERFACCIA) -> List[str]:
    """Percorsi dei dispositivi presenti con l'interfaccia di configurazione di PS-RX."""
    setupapi, _, _ = _api()
    guid = GUID.da_testo(guid_testo)
    h = setupapi.SetupDiGetClassDevsW(ctypes.byref(guid), None, None, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)
    if h in (None, INVALID_HANDLE_VALUE):
        return []
    percorsi = []
    try:
        i = 0
        while True:
            dati = SP_DEVICE_INTERFACE_DATA()
            dati.cbSize = ctypes.sizeof(SP_DEVICE_INTERFACE_DATA)
            if not setupapi.SetupDiEnumDeviceInterfaces(h, None, ctypes.byref(guid), i, ctypes.byref(dati)):
                break
            richiesto = wt.DWORD(0)
            setupapi.SetupDiGetDeviceInterfaceDetailW(h, ctypes.byref(dati), None, 0, ctypes.byref(richiesto), None)
            buffer = ctypes.create_string_buffer(richiesto.value)
            # SP_DEVICE_INTERFACE_DETAIL_DATA_W: cbSize = 8 su 64 bit, 6 su 32 bit; poi la stringa.
            ctypes.c_uint32.from_buffer(buffer).value = 8 if ctypes.sizeof(ctypes.c_void_p) == 8 else 6
            if setupapi.SetupDiGetDeviceInterfaceDetailW(h, ctypes.byref(dati), buffer, richiesto, None, None):
                percorsi.append(ctypes.wstring_at(ctypes.addressof(buffer) + 4))
            i += 1
    finally:
        setupapi.SetupDiDestroyDeviceInfoList(h)
    return percorsi


class TrasportoWinUsb:
    """Richieste di controllo vendor sull'interfaccia WinUSB di PS-RX."""

    def __init__(self, percorso: str):
        self.percorso = percorso
        _, self._winusb, self._kernel32 = _api()
        self._file = self._kernel32.CreateFileW(percorso, GENERIC_READ | GENERIC_WRITE,
                                                FILE_SHARE_READ | FILE_SHARE_WRITE, None, OPEN_EXISTING,
                                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, None)
        if self._file in (None, INVALID_HANDLE_VALUE):
            raise Scollegato(f'{percorso} (errore {ctypes.get_last_error()})')
        self._usb = ctypes.c_void_p()
        if not self._winusb.WinUsb_Initialize(self._file, ctypes.byref(self._usb)):
            errore = ctypes.get_last_error()
            self._kernel32.CloseHandle(self._file)
            self._file = None
            raise Scollegato(f'WinUsb_Initialize: errore {errore}')

    def chiudi(self) -> None:
        if getattr(self, '_usb', None):
            self._winusb.WinUsb_Free(self._usb)
            self._usb = None
        if getattr(self, '_file', None):
            self._kernel32.CloseHandle(self._file)
            self._file = None

    def _controllo(self, tipo: int, comando: int, indice: int, buffer, lunghezza: int, timeout_ms: int) -> int:
        if not self._usb:
            raise Scollegato(self.percorso)
        t = wt.ULONG(timeout_ms)
        self._winusb.WinUsb_SetPipePolicy(self._usb, 0, PIPE_TRANSFER_TIMEOUT, ctypes.sizeof(t), ctypes.byref(t))
        setup = WINUSB_SETUP_PACKET(tipo, p.RICHIESTA, comando, indice, lunghezza)
        trasferiti = wt.ULONG(0)
        if not self._winusb.WinUsb_ControlTransfer(self._usb, setup, buffer, lunghezza, ctypes.byref(trasferiti), None):
            errore = ctypes.get_last_error()
            if errore == ERROR_GEN_FAILURE:
                raise Stallo(comando)
            if errore in ERRORI_SCOLLEGATO:
                raise Scollegato(f'{self.percorso} (errore {errore})')
            raise OSError(errore, f'WinUsb_ControlTransfer: errore {errore}')
        return trasferiti.value

    def leggi(self, comando: int, indice: int = 0, lunghezza: int = 2304, timeout_ms: int = 1000) -> bytes:
        buffer = ctypes.create_string_buffer(lunghezza)
        n = self._controllo(0xC0, comando, indice, buffer, lunghezza, timeout_ms)
        return buffer.raw[:n]

    def scrivi(self, comando: int, indice: int = 0, dati: bytes = b'', timeout_ms: int = 2000) -> None:
        buffer = ctypes.create_string_buffer(dati, len(dati)) if dati else None
        self._controllo(0x40, comando, indice, buffer, len(dati), timeout_ms)


def apri() -> TrasportoWinUsb:
    for percorso in percorsi_interfaccia():
        try:
            t = TrasportoWinUsb(percorso)
        except Scollegato:
            continue
        try:
            if t.leggi(p.CMD_INFO, lunghezza=p.DIM_INFO)[:4] == p.MAGIC:
                return t
        except (Stallo, Scollegato, OSError):
            pass
        t.chiudi()
    raise Scollegato('nessun PS-RX collegato')
