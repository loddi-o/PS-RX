"""PS-RX - eccezioni dei trasporti USB e del client."""


class Stallo(Exception):
    """Il Pico ha rifiutato la richiesta (STALL): il motivo si legge con CMD_ERRORE."""


class Scollegato(Exception):
    """Nessun PS-RX, oppure il dispositivo e' sparito (per esempio il Pico si e' ricollegato all'USB)."""


class PermessoNegato(Exception):
    """Linux: manca il permesso di scrittura sul nodo usbfs (serve la regola udev o root)."""


class ErrorePsrx(Exception):
    """Richiesta rifiutata dal Pico, con il codice del motivo."""

    def __init__(self, codice: int, comando: int, testo: str):
        super().__init__(testo)
        self.codice = codice
        self.comando = comando
