"""
PS-RX app - scheda Sistema: impostazioni del ricevitore, firmware, registro e opzioni dell'app.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QFontDatabase
from PySide6.QtWidgets import (QCheckBox, QComboBox, QDialog, QFileDialog, QGroupBox, QLabel, QPlainTextEdit, QProgressDialog,
                               QPushButton, QVBoxLayout)

from psrx import protocollo as p
from psrx import aggiornamenti as ag
from psrx.firmware import FirmwareNonValido, leggi as leggi_firmware
from psrx.servizio import Interrotto

from . import aggiorna, opzioni
from .controlli import nota, riga
from .lavoratore import Istantanea, testo_errore
from .scheda_gamepad import Scheda
from psrx.lingua import tr


def _durata(s: int) -> str:
    if s >= 86400:
        return tr('{0} g {1} h', s // 86400, s % 86400 // 3600)
    if s >= 3600:
        return f'{s // 3600} h {s % 3600 // 60} min'
    return f'{s // 60} min' if s >= 60 else f'{s} s'


class DialogoRegistro(QDialog):
    def __init__(self, scheda: 'SchedaSistema'):
        super().__init__(scheda)
        self.scheda = scheda
        self.setWindowTitle(tr('Registro del ricevitore'))
        self.resize(820, 480)
        v = QVBoxLayout(self)
        self.testo = QPlainTextEdit()
        self.testo.setReadOnly(True)
        self.testo.setFont(QFontDatabase.systemFont(QFontDatabase.FixedFont))
        v.addWidget(self.testo)
        aggiorna = QPushButton(tr('Aggiorna'))
        aggiorna.clicked.connect(self.carica)
        v.addLayout(riga(aggiorna))
        self.carica()

    def carica(self) -> None:
        def fatto(r):
            attivo, testo = r
            self.testo.setPlainText(testo if attivo else tr('Registro spento: attiva "Registro diagnostico" qui sotto nella scheda Sistema.'))
            self.testo.verticalScrollBar().setValue(self.testo.verticalScrollBar().maximum())
        self.scheda.finestra.esegui(lambda c: c.registro(), '', fatto)


class SchedaSistema(Scheda):
    avanzamento = Signal(str, int, int)

    def __init__(self, finestra):
        super().__init__(finestra)
        gruppo = QGroupBox(tr('Ricevitore'))
        v = QVBoxLayout(gruppo)
        self.info = QLabel('-')
        self.info.setWordWrap(True)
        self.info.setTextInteractionFlags(Qt.TextSelectableByMouse)
        v.addWidget(self.info)
        self.col.addWidget(gruppo)

        gruppo = QGroupBox(tr('Impostazioni del ricevitore'))
        v = QVBoxLayout(gruppo)
        self.globali = self.controlli_globali('sistema', v)
        self.predefinite = QPushButton(tr('Impostazioni di fabbrica'))
        self.predefinite.setToolTip(tr('Reti WiFi, Wake-on-LAN, abbinamenti e impostazioni dei controller restano'))
        self.predefinite.clicked.connect(self._predefinite)
        self.salva = QPushButton(tr('Salva ora'))
        self.salva.setToolTip(tr('Le modifiche si salvano da sole appena non ci sono controller collegati'))
        self.salva.clicked.connect(lambda: self.esegui(lambda c: c.salva_ora(), tr('Impostazioni salvate')))
        v.addLayout(riga(self.salva, self.predefinite))
        self.col.addWidget(gruppo)

        gruppo = QGroupBox(tr('Firmware'))
        v = QVBoxLayout(gruppo)
        self.aggiorna_fw = QPushButton(tr('Aggiorna il firmware…'))
        self.aggiorna_fw.clicked.connect(self._aggiorna_firmware)
        self.bootsel = QPushButton(tr('Riavvia in modalità aggiornamento (BOOTSEL)'))
        self.bootsel.clicked.connect(self._bootsel)
        self.registro = QPushButton(tr('Registro…'))
        self.registro.clicked.connect(lambda: DialogoRegistro(self).exec())
        self.procedura = QPushButton(tr('Prepara un nuovo ricevitore…'))
        self.procedura.setToolTip(tr('Firmware su un Pico 2 W nuovo, o di nuovo su questo ricevitore'))
        self.procedura.clicked.connect(finestra.apri_procedura)
        v.addLayout(riga(self.aggiorna_fw, self.bootsel, self.registro))
        v.addLayout(riga(self.procedura))
        v.addWidget(nota(tr("L'aggiornamento dall'app carica il file (.uf2 o .bin), ne verifica l'impronta e lo installa: serve che i controller siano spenti. In modalità BOOTSEL il Pico compare come chiavetta e il file .uf2 si copia a mano.")))
        self.col.addWidget(gruppo)

        gruppo = QGroupBox(tr('Aggiornamenti (GitHub)'))
        v = QVBoxLayout(gruppo)
        self.stato_agg = QLabel(tr('Nessuna ricerca in questa sessione.'))
        self.stato_agg.setWordWrap(True)
        self.stato_agg.setOpenExternalLinks(True)
        self.stato_agg.setTextFormat(Qt.RichText)
        v.addWidget(self.stato_agg)
        self.cerca = QPushButton(tr('Cerca aggiornamenti'))
        self.cerca.clicked.connect(self._cerca)
        self.installa_fw = QPushButton(tr('Installa il firmware'))
        self.installa_fw.clicked.connect(self._scarica_firmware)
        self.aggiorna_app = QPushButton(tr("Aggiorna l'app"))
        self.aggiorna_app.clicked.connect(self._scarica_app)
        for w in (self.installa_fw, self.aggiorna_app):
            w.setVisible(False)
        v.addLayout(riga(self.cerca, self.installa_fw, self.aggiorna_app))
        self.automatico = QCheckBox(tr("Cerca aggiornamenti all'avvio (al massimo una volta al giorno)"))
        self.automatico.setChecked(opzioni.leggi('cerca_aggiornamenti', True))
        self.automatico.toggled.connect(lambda v_: opzioni.scrivi('cerca_aggiornamenti', v_))
        v.addWidget(self.automatico)
        self.col.addWidget(gruppo)
        agg = finestra.aggiornatore
        agg.esito.connect(self._su_esito)
        agg.avanzamento.connect(self._su_download)
        agg.scaricato.connect(self._su_scaricato)

        gruppo = QGroupBox(tr('App'))
        v = QVBoxLayout(gruppo)
        self.avvio = QCheckBox(tr("Avvia con Windows (nell'area di notifica)"))
        self.avvio.setChecked(opzioni.avvio_automatico())
        self.avvio.toggled.connect(opzioni.imposta_avvio_automatico)
        self.notifiche = QCheckBox(tr('Notifiche di collegamento e batteria'))
        self.notifiche.setChecked(opzioni.leggi('notifiche', True))
        self.notifiche.toggled.connect(lambda v_: opzioni.scrivi('notifiche', v_))
        self.vassoio = QCheckBox(tr("Chiudendo la finestra l'app resta nell'area di notifica"))
        self.vassoio.setChecked(opzioni.leggi('resta_in_vassoio', True))
        self.vassoio.toggled.connect(lambda v_: opzioni.scrivi('resta_in_vassoio', v_))
        for w in (self.avvio, self.notifiche, self.vassoio):
            v.addWidget(w)
        self.lingua = QComboBox()
        for codice, nome in (('auto', tr('Automatica (lingua di Windows)')), ('it', 'Italiano'), ('en', 'English')):
            self.lingua.addItem(nome, codice)
        indice = self.lingua.findData(str(opzioni.leggi('lingua', 'auto')))
        self.lingua.setCurrentIndex(max(indice, 0))
        self.lingua.activated.connect(self._cambia_lingua)
        v.addLayout(riga(QLabel(tr('Lingua / Language')), self.lingua))
        if not opzioni.AVVIO_DISPONIBILE:
            self.avvio.setEnabled(False)
        self.col.addWidget(gruppo)
        self.col.addStretch(1)

        self._progresso: Optional[QProgressDialog] = None
        self._interrotto = False
        self.avanzamento.connect(self._su_avanzamento)

    def aggiorna(self, ist: Optional[Istantanea]) -> None:
        super().aggiorna(ist)
        attivo = ist is not None and ist.stato is not None and ist.info is not None
        for w in (self.predefinite, self.salva, self.aggiorna_fw, self.bootsel, self.registro, self.installa_fw):
            w.setEnabled(attivo)
        for ident, c in self.globali.items():
            c.mostra(ist.impostazioni.get(ident) if attivo else None)
        if not attivo:
            self.info.setText(tr('Ricevitore non collegato'))
            return
        self._riallinea_ricerca(ist.info.versione)
        i, st = ist.info, ist.stato
        usb = tr("{0} {1} sull'USB", st.usb_gamepad, tr('controller Xbox') if st.modalita == 1 else 'gamepad')
        if st.usb_sospeso:
            usb += tr(' (PC in sospensione)')
        audio = [tr(n) for n, a in (('altoparlante', st.altoparlante), ('microfono', st.microfono)) if a]
        righe = [
            tr('<b>PS-RX {0}</b> · base {1} · acceso da {2}', i.versione, i.base, _durata(st.uptime_s)),
            tr('Modalità {0} · {1} · audio: {2}', st.nome_modalita, usb, ', '.join(audio) or tr('non in uso')),
            tr('Memoria libera {0} KB', st.heap_libero // 1024) + (tr(' · modifiche in attesa di salvataggio (a controller spenti)') if st.salvataggio_in_sospeso else ''),
        ]
        self.info.setText('<br>'.join(righe))

    def _cambia_lingua(self, _indice: int) -> None:
        opzioni.scrivi('lingua', self.lingua.currentData())
        if aggiorna.exe_sostituibile() and self.finestra.conferma(
                'PS-RX', tr("La lingua cambia al riavvio dell'app. Riavviarla adesso?")):
            self.finestra.riavvia_app()
        else:
            self.finestra.messaggio(tr("La lingua cambia al prossimo avvio dell'app"))

    def _predefinite(self) -> None:
        if self.finestra.conferma(tr('Impostazioni di fabbrica?'),
                                  tr('Le impostazioni del ricevitore tornano quelle iniziali (modalità PlayStation, posti dinamici...). Reti WiFi, Wake-on-LAN e controller abbinati restano.')):
            self.esegui(lambda c: c.predefinite(), tr('Impostazioni di fabbrica ripristinate'))

    def _bootsel(self) -> None:
        if self.finestra.conferma(tr('Riavviare in modalità aggiornamento?'),
                                  tr('Il ricevitore si riavvia come chiavetta "RP2350": copia il file .uf2 al suo interno. Servono i controller spenti.')):
            self.esegui(lambda c: c.bootsel_ora(), tr('Ricevitore in modalità BOOTSEL'))

    # --- aggiornamenti da GitHub ------------------------------------------------------------------
    def _versione_firmware(self) -> Optional[str]:
        return self.ist.info.versione if self.ist and self.ist.info else None

    def _riallinea_ricerca(self, versione: str) -> None:
        """L'esito dell'ultima ricerca segue la versione attuale del ricevitore (per esempio appena aggiornato)."""
        c = self.finestra.aggiornatore.ultimo
        if c is None or c.firmware_installato == versione:
            return
        c.firmware_installato = versione
        c.firmware_nuovo = ag.piu_nuova(c.release.versione, versione) and ag.file_firmware(c.release) is not None
        self._su_esito(c, '')

    def _cerca(self) -> None:
        if self.finestra.aggiornatore.cerca(self._versione_firmware()):
            self.cerca.setEnabled(False)
            self.stato_agg.setText(tr('Ricerca su GitHub…'))

    def _su_esito(self, c, errore: str) -> None:
        self.cerca.setEnabled(True)
        if c is None:
            self.stato_agg.setText(tr('Ricerca non riuscita: {0}', errore))
            return
        rel = c.release
        righe = [tr('Ultima versione: <b>{0}</b> · <a href="{1}">note della release</a>', rel.versione, rel.pagina)]
        fw = c.firmware_installato or tr('ricevitore non collegato')
        righe.append(tr('Firmware del ricevitore: {0}', fw) + (tr(' → <b>aggiornamento disponibile</b>') if c.firmware_nuovo else ''))
        righe.append(f'App: {c.app_installata}' + (tr(' → <b>aggiornamento disponibile</b>') if c.app_nuova else ''))
        if c.app_nuova and not aggiorna.exe_sostituibile():
            righe.append(tr("L'app gira da sorgente: aggiornala dalla repo (git pull)."))
        self.stato_agg.setText('<br>'.join(righe))
        self.installa_fw.setText(tr('Installa il firmware {0}', rel.versione))
        self.installa_fw.setVisible(c.firmware_nuovo)
        self.aggiorna_app.setText(tr("Aggiorna l'app a {0}", rel.versione))
        self.aggiorna_app.setVisible(c.app_nuova and aggiorna.exe_sostituibile())

    def _avvia_download(self, tipo: str, f, titolo: str) -> None:
        self._interrotto = False
        self.finestra.aggiornatore.interrompi = False
        self._progresso = QProgressDialog(tr('Download di {0}…', f.nome), tr('Annulla'), 0, 100, self)
        self._progresso.setWindowTitle(titolo)
        self._progresso.setWindowModality(Qt.WindowModal)
        self._progresso.setMinimumDuration(0)
        self._progresso.canceled.connect(self._annulla)
        self._progresso.show()
        self.finestra.aggiornatore.scarica(tipo, f)

    def _scarica_firmware(self) -> None:
        c = self.finestra.aggiornatore.ultimo
        f = ag.file_firmware(c.release) if c else None
        if f is None:
            return
        self._avvia_download('firmware', f, tr('Aggiornamento del firmware'))

    def _scarica_app(self) -> None:
        c = self.finestra.aggiornatore.ultimo
        if not c or 'app_windows' not in c.release.file:
            return
        self._avvia_download('app', c.release.file['app_windows'], tr("Aggiornamento dell'app"))

    def _su_download(self, fatti: int, totali: int) -> None:
        if self._progresso and totali:
            self._progresso.setValue(int(fatti * 100 / totali))

    def _su_scaricato(self, tipo: str, percorso: str, errore: str) -> None:
        self._chiudi_progresso()
        if errore:
            self.finestra.messaggio(tr('Download non riuscito: {0}', errore), errore=not self._interrotto)
            return
        if tipo == 'firmware':
            self._installa_firmware(percorso)
            return
        if self.finestra.conferma(tr("Aggiornare l'app?"), tr("Windows chiede il permesso di installare; poi l'app si chiude e si riapre da sola con la versione nuova.")):
            try:
                aggiorna.esegui_installer(percorso)
            except OSError as e:
                self.finestra.messaggio(tr("Aggiornamento dell'app non riuscito: {0}", e), errore=True)
                return
            self.finestra.riavvia()

    # --- aggiornamento del firmware -------------------------------------------------------------
    def _aggiorna_firmware(self) -> None:
        percorso, _ = QFileDialog.getOpenFileName(self, tr('Firmware PS-RX'), '', tr('Firmware (*.uf2 *.bin)'))
        if percorso:
            self._installa_firmware(percorso)

    def _installa_firmware(self, percorso: str) -> None:
        capacita = self.ist.info.staging_max if self.ist and self.ist.info else None
        try:
            img = leggi_firmware(percorso, capacita)
        except (FirmwareNonValido, OSError) as e:
            self.finestra.messaggio(tr('File non valido: {0}', e), errore=True)
            return
        attuale = self.ist.info.versione if self.ist and self.ist.info else '?'
        if not self.finestra.conferma(tr('Aggiornare il firmware?'),
                                      tr('Installata: {0}\nNuova: {1} ({2} KB)\n\nIl ricevitore si riavvia alla fine.', attuale, img.versione or tr('sconosciuta'), img.dimensione // 1024)):
            return
        self._interrotto = False
        self._progresso = QProgressDialog(tr('Preparazione…'), tr('Annulla'), 0, 100, self)
        self._progresso.setWindowTitle(tr('Aggiornamento del firmware'))
        self._progresso.setWindowModality(Qt.WindowModal)
        self._progresso.setMinimumDuration(0)
        self._progresso.canceled.connect(self._annulla)
        self._progresso.show()

        def avanz(fase, fatto, totale):
            self.avanzamento.emit(fase, fatto, totale)

        def fatto(_):
            self._chiudi_progresso()
            self.finestra.messaggio(tr('Firmware installato: il ricevitore si riavvia'))

        def errore(e):
            self._chiudi_progresso()
            if isinstance(e, Interrotto):
                self.finestra.messaggio(tr('Aggiornamento annullato'))
            else:
                self.finestra.messaggio(tr('Aggiornamento non riuscito: {0}', testo_errore(e)), errore=True)

        self.finestra.ponte.esegui(lambda c: c.carica_firmware(img, avanz, lambda: self._interrotto), fatto, errore)

    def _annulla(self) -> None:
        self._interrotto = True
        self.finestra.aggiornatore.interrompi = True

    def _chiudi_progresso(self) -> None:
        if self._progresso:
            self._progresso.canceled.disconnect(self._annulla)
            self._progresso.close()
            self._progresso = None

    def _su_avanzamento(self, fase: str, fatto: int, totale: int) -> None:
        if not self._progresso:
            return
        testi = {
            'attesa_pad': tr('Spegni i controller: il firmware si carica e si installa solo senza controller collegati.'),
            'caricamento': tr('Caricamento: blocco {0} di {1}', fatto, totale),
            'verifica': tr("Verifica dell'impronta SHA-256…"),
            'installazione': tr('Installazione: il ricevitore si riavvia (non staccarlo)…'),
        }
        self._progresso.setLabelText(testi.get(fase, fase))
        if fase == 'caricamento' and totale:
            self._progresso.setValue(int(fatto * 90 / totale))
        elif fase == 'verifica':
            self._progresso.setValue(95)
        elif fase == 'installazione':
            self._progresso.setValue(99)
