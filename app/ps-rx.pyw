"""Avvio dell'app PS-RX senza console (sviluppo: pythonw ps-rx.pyw; l'exe usa lo stesso punto d'ingresso)."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from psrx_app.__main__ import main  # noqa: E402

sys.exit(main(sys.argv[1:]))
