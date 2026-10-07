"""
PS-RX - traduzioni inglesi (chiave: testo italiano). Usate da lingua.tr(); un test controlla che ogni testo
dell'app, della libreria e dello schema sia qui (strumenti/chiavi_lingua.py --mancanti).
"""

EN = {
    # --- aggiornamenti ---------------------------------------------------------------------------------
    'Ultima release: {0} ({1})': 'Latest release: {0} ({1})',
    'download incompleto ({0} di {1} byte)': 'incomplete download ({0} of {1} bytes)',
    'impronta SHA-256 diversa da quella pubblicata: file rovinato':
        'SHA-256 hash differs from the published one: damaged file',
    'Firmware: installato {0}': 'Firmware: installed {0}',
    'App: installata {0}': 'App: installed {0}',
    "GitHub ha risposto con l'errore {0}": 'GitHub answered with error {0}',
    'GitHub non raggiungibile ({0})': 'GitHub not reachable ({0})',
    'download non riuscito ({0})': 'download failed ({0})',
    ' -> aggiornamento disponibile': ' -> update available',
    ', aggiornato': ', up to date',
    ', aggiornata': ', up to date',
    'nessuna release pubblicata su GitHub': 'no release published on GitHub',
    "GitHub ha limitato le richieste: riprova tra un'ora": 'GitHub is rate-limiting requests: try again in an hour',
    'download annullato': 'download cancelled',

    # --- riga di comando --------------------------------------------------------------------------------
    'PS-RX {0} (protocollo {1}, base {2}), acceso da {3}': 'PS-RX {0} (protocol {1}, base {2}), up for {3}',
    'Modalità: {0}; USB: {1} gamepad esposti{2}{3}': 'Mode: {0}; USB: {1} gamepads exposed{2}{3}',
    ', IP {0}, segnale {1} dBm': ', IP {0}, signal {1} dBm',
    'WiFi: {0}; ultimo Wake-on-LAN: {1}': 'WiFi: {0}; last Wake-on-LAN: {1}',
    'Audio: {0}{1}': 'Audio: {0}{1}',
    '{0}: {1} byte, versione {2}': '{0}: {1} bytes, version {2}',
    '\nfatto: il ricevitore si riavvia col firmware nuovo (circa 15 s, non staccarlo)':
        '\ndone: the receiver restarts with the new firmware (about 15 s, do not unplug it)',
    "collega il Pico 2 W (se e' gia' stato usato, tieni premuto BOOTSEL mentre lo colleghi)...":
        'plug in the Pico 2 W (if it was used before, hold BOOTSEL while plugging it in)...',
    'copia di {0} (versione {1}) nel Pico...': 'copying {0} (version {1}) to the Pico...',
    'attendo il ricevitore PS-RX...': 'waiting for the PS-RX receiver...',
    "fatto: il ricevitore e' pronto (python -m psrx abbina per il primo controller)":
        'done: the receiver is ready (python -m psrx abbina for the first controller)',
    'Finestra di abbinamento aperta': 'Pairing window open',
    'Impostazioni in attesa di essere salvate (succede senza controller collegati)':
        'Settings waiting to be saved (happens with no controllers connected)',
    'Memoria libera: {0} KB': 'Free memory: {0} KB',
    'Memoria libera: -': 'Free memory: -',
    '  posto {0}: {1} {2} {3} batteria {4}, segnale {5}, {6} report/s, polling {7} Hz':
        '  slot {0}: {1} {2} {3} battery {4}, signal {5}, {6} reports/s, polling {7} Hz',
    'la release non contiene il firmware': 'the release does not contain the firmware',
    'il firmware installato è già il più recente': 'the installed firmware is already the latest',
    'Gestione del ricevitore PS-RX via USB': 'Manage the PS-RX receiver over USB',
    'usa un ricevitore simulato': 'use a simulated receiver',
    'impostazione per controller': 'per-controller setting',
    'cerca le reti WiFi visibili dal ricevitore (solo 2,4 GHz)':
        'scan the WiFi networks visible to the receiver (2.4 GHz only)',
    'prova una rete salvata (password, indirizzo, internet)': 'test a saved network (password, address, internet)',
    'cerca una nuova versione su GitHub': 'check GitHub for a new version',
    "scarica da GitHub e installa l'ultimo firmware": 'download and install the latest firmware from GitHub',
    'firmware su un Pico 2 W nuovo o in BOOTSEL': 'firmware on a new Pico 2 W or one in BOOTSEL mode',
    "firmware .uf2 (senza: l'ultimo da GitHub)": 'firmware .uf2 (omitted: the latest from GitHub)',
    ', posti fissi': ', fixed slots',
    'non in uso': 'not in use',
    ' (microfono muto)': ' (microphone muted)',
    'spegni i controller per continuare': 'turn off the controllers to continue',
    'caricamento {0}/{1}': 'uploading {0}/{1}',
    'installazione: il ricevitore si riavvia': 'installing: the receiver restarts',
    'ricerca non riuscita: {0}': 'check failed: {0}',
    '\rdownload {0}%': '\rdownload {0}%',
    "nessun Pico in modalita' BOOTSEL": 'no Pico in BOOTSEL mode',
    "l'ultima release non contiene il firmware .uf2": 'the latest release does not contain the .uf2 firmware',
    'manca il permesso sul nodo USB {0}: python -m psrx regola-udev | sudo tee {1}/etc/udev/rules.d/70-ps-rx.rules':
        'no permission on USB node {0}: python -m psrx regola-udev | sudo tee {1}/etc/udev/rules.d/70-ps-rx.rules',
    'ricevitore PS-RX non trovato ({0}). È collegato via USB?': 'PS-RX receiver not found ({0}). Is it plugged in via USB?',
    'il ricevitore ha rifiutato: {0}': 'the receiver refused: {0}',
    'firmware non valido: {0}': 'invalid firmware: {0}',
    'installazione non riuscita: {0}': 'installation failed: {0}',
    'impostazione sconosciuta: {0} (vedi "impostazioni")': 'unknown setting: {0} (see "impostazioni")',
    'collegato (posto {0})': 'connected (slot {0})',
    'non collegato': 'not connected',
    '{0}  {1:<16} {2}; audio {3}, microfono {4}, {5} Hz, touchpad come mouse {6}':
        '{0}  {1:<16} {2}; audio {3}, microphone {4}, {5} Hz, touchpad as mouse {6}',
    '(senza nome)': '(no name)',
    'sì': 'yes',
    'finestra di abbinamento di 30 s: tieni Create + PS (DualSense) o Share + PS (DualShock 4)':
        '30 s pairing window: hold Create + PS (DualSense) or Share + PS (DualShock 4)',
    'Wake-on-LAN: {0}{1}': 'Wake-on-LAN: {0}{1}',
    'nessuna destinazione': 'no target',
    ' (disattivato)': ' (disabled)',
    'nessuna rete rilevata (il ricevitore vede solo le reti a 2,4 GHz)':
        'no networks found (the receiver only sees 2.4 GHz networks)',
    '{0:<33} {1:>4} dBm  canale {2:>2}  {3}': '{0:<33} {1:>4} dBm  channel {2:>2}  {3}',
    'aperta': 'open',
    'protetta': 'secured',
    'pacchetti magici inviati': 'magic packets sent',
    'in ascolto (Ctrl+C per uscire)': 'listening (Ctrl+C to quit)',
    'registro spento: python -m psrx imposta registro 1': 'log disabled: python -m psrx imposta registro 1',
    "il Pico non si e' presentato come PS-RX": 'the Pico did not show up as PS-RX',
    "il disco montato non e' il bootloader dell'RP2350": 'the mounted disk is not the RP2350 bootloader',

    # --- protocollo: errori, stati, eventi ---------------------------------------------------------------
    'nessun errore': 'no error',
    'comando sconosciuto': 'unknown command',
    'lunghezza dei dati sbagliata': 'wrong data length',
    'valore non ammesso': 'value not allowed',
    "il ricevitore sta ancora finendo l'operazione precedente": 'the receiver is still finishing the previous operation',
    'spegni prima i controller (PS-RX scrive in memoria solo senza controller collegati)':
        'turn off the controllers first (PS-RX only writes its memory with no controllers connected)',
    'controller non trovato fra gli abbinati': 'controller not found among the paired ones',
    'posti esauriti': 'no free slots',
    'scrittura in memoria fallita': 'memory write failed',
    'blocco fuori ordine o caricamento non iniziato': 'block out of order or upload not started',
    'firmware di dimensione non valida': 'invalid firmware size',
    'impronta SHA-256 diversa: firmware rovinato durante il caricamento':
        'SHA-256 hash differs: firmware damaged during the upload',
    "il file non e' un firmware per Raspberry Pi Pico 2 W": 'the file is not a Raspberry Pi Pico 2 W firmware',
    'nessun firmware pronto da installare': 'no firmware ready to install',
    'WiFi non connesso': 'WiFi not connected',
    'PlayStation': 'PlayStation',
    'Xbox': 'Xbox',
    'Steam Controller': 'Steam Controller',
    '-': '-',
    'DualSense': 'DualSense',
    'DualSense Edge': 'DualSense Edge',
    'DualShock 4': 'DualShock 4',
    'spento (radio al Bluetooth)': 'off (radio given to Bluetooth)',
    'connessione in corso': 'connecting',
    'connesso': 'connected',
    'nessuna rete raggiungibile': 'no reachable network',
    'collegamento alla rete': 'connecting to the network',
    'indirizzo IP dal router': 'IP address from the router',
    'verifica di internet': 'checking internet',
    'tutto a posto: password giusta, indirizzo dal router e internet raggiungibile':
        'all good: correct password, address from the router and internet reachable',
    'password sbagliata': 'wrong password',
    'rete non trovata: è spenta, troppo lontana, il nome è sbagliato o trasmette solo a 5 GHz (il Pico 2 W usa solo i 2,4 GHz)':
        'network not found: it is off, too far away, the name is wrong or it only broadcasts at 5 GHz '
        '(the Pico 2 W only uses 2.4 GHz)',
    'il router non ha completato il collegamento (riprova; controlla WPA2/WPA3)':
        'the router did not complete the connection (try again; check WPA2/WPA3)',
    'collegato, ma il router non ha dato un indirizzo IP (DHCP spento o pieno?)':
        'connected, but the router gave no IP address (DHCP off or full?)',
    'rete di casa raggiungibile, internet no (il Wake-on-LAN funziona lo stesso: resta in casa)':
        'home network reachable, internet not (Wake-on-LAN still works: it stays on the local network)',
    'prova interrotta: si è collegato un controller e il WiFi si è spento':
        'test interrupted: a controller connected and the WiFi turned off',
    'Controller {0}': 'Controller {0}',
    'batteria sconosciuta': 'battery unknown',
    'batteria {0}%': 'battery {0}%',
    'evento {0}': 'event {0}',
    'MAC non valido: {0}': 'invalid MAC: {0}',
    'nessun firmware in caricamento': 'no firmware being uploaded',
    'ricezione: {0}/{1} blocchi': 'receiving: {0}/{1} blocks',
    'scrittura: {0}/{1} blocchi': 'writing: {0}/{1} blocks',
    "verifica dell'impronta SHA-256": 'checking the SHA-256 hash',
    'firmware verificato, pronto da installare': 'firmware verified, ready to install',
    'installazione in corso (non staccare il ricevitore)': 'installing (do not unplug the receiver)',
    'errore: {0}': 'error: {0}',
    'nessuna prova': 'no test',
    'in corso: {0}…': 'in progress: {0}…',
    'esito {0}': 'result {0}',
    ' (IP {0}, segnale {1} dBm, internet in {2} ms)': ' (IP {0}, signal {1} dBm, internet in {2} ms)',
    "non e' un PS-RX": 'not a PS-RX',
    '{0} collegato': '{0} connected',
    'Posto {0} · {1} · modalità {2} · {3}': 'Slot {0} · {1} · {2} mode · {3}',
    '{0} scollegato': '{0} disconnected',
    'Posto {0}': 'Slot {0}',
    'Batteria in esaurimento': 'Battery running low',
    '{0}: {1}': '{0}: {1}',
    'Batteria quasi scarica': 'Battery almost empty',
    '{0}: {1}, collega il cavo': '{0}: {1}, plug in the cable',
    'altoparlante': 'speaker',
    'microfono': 'microphone',

    # --- app: finestra e area di notifica ----------------------------------------------------------------
    'PS-RX: impostazioni e notifiche del ricevitore': 'PS-RX: receiver settings and notifications',
    "parti solo nell'area di notifica": 'start in the notification area only',
    'Ricerca del ricevitore…': 'Looking for the receiver…',
    'Prepara un nuovo ricevitore…': 'Set up a new receiver…',
    'Installa PS-RX su un Raspberry Pi Pico 2 W nuovo o in modalità BOOTSEL':
        'Install PS-RX on a new Raspberry Pi Pico 2 W or one in BOOTSEL mode',
    'Gamepad': 'Gamepad',
    'Rete': 'Network',
    'Sistema': 'System',
    "Il ricevitore si ricollega all'USB per cambiare forma": 'The receiver reconnects to USB to change its shape',
    'PS-RX: aggiornamento disponibile': 'PS-RX: update available',
    "Versione {0} ({1}). Apri l'app → Sistema → Aggiornamenti.": 'Version {0} ({1}). Open the app → System → Updates.',
    'Apri PS-RX': 'Open PS-RX',
    'Abbina un nuovo controller': 'Pair a new controller',
    'Esci': 'Quit',
    ': per circa un secondo i {0} controller collegati non arrivano al PC. Continuare?':
        ': for about a second the {0} connected controllers will not reach the PC. Continue?',
    '. Continuare?': '. Continue?',
    "<b>Ricevitore non trovato.</b> Collega il PS-RX a una porta USB (se è aperto da un'altra app, chiudila). Hai un Pico 2 W nuovo? Usa il pulsante qui accanto.":
        '<b>Receiver not found.</b> Plug the PS-RX into a USB port (if another app has it open, close it). '
        'Got a new Pico 2 W? Use the button next to this.',
    '<b>PS-RX collegato</b> · modalità {0} · {1} controller collegato':
        '<b>PS-RX connected</b> · {0} mode · {1} controller connected',
    '<b>PS-RX collegato</b> · modalità {0} · {1} controller collegati':
        '<b>PS-RX connected</b> · {0} mode · {1} controllers connected',
    ' · <b>abbinamento in corso</b>': ' · <b>pairing in progress</b>',
    ' (simulato)': ' (simulated)',
    'PS-RX: non collegato': 'PS-RX: not connected',
    'PS-RX: {0} controller, modalità {1}': 'PS-RX: {0} controllers, {1} mode',
    'Trovato un Raspberry Pi Pico pronto per il firmware.': 'Found a Raspberry Pi Pico ready for the firmware.',
    "L'app resta qui per le notifiche. Per chiuderla: tasto destro → Esci.":
        'The app stays here for notifications. To close it: right click → Quit.',
    'Abbinamento aperto per 30 secondi': 'Pairing open for 30 seconds',
    "Ricevitore non trovato (scollegato, oppure aperto da un'altra app)":
        'Receiver not found (unplugged, or open in another app)',
    'Permesso negato sul dispositivo USB': 'Permission denied on the USB device',
    'il firmware del ricevitore è troppo vecchio per questa funzione: aggiornalo da Sistema → Aggiornamenti':
        "the receiver's firmware is too old for this feature: update it from System → Updates",

    # --- app: nuovo ricevitore ----------------------------------------------------------------------------
    'Prepara un nuovo ricevitore': 'Set up a new receiver',
    '1. Collega il Raspberry Pi Pico 2 W': '1. Plug in the Raspberry Pi Pico 2 W',
    'Riflasha il ricevitore PS-RX collegato': 'Reflash the connected PS-RX receiver',
    'Abbina il primo controller': 'Pair the first controller',
    'Chiudi': 'Close',
    'Collega il Pico a una porta USB di questo PC.<br>• <b>Pico nuovo</b>: si presenta da solo come chiavetta "RP2350".<br>• <b>Pico già usato</b>: tieni premuto il tasto <b>BOOTSEL</b> mentre colleghi il cavo, poi rilascialo.<br><br>In attesa del Pico…':
        'Plug the Pico into a USB port of this PC.<br>• <b>New Pico</b>: it shows up by itself as an "RP2350" '
        'drive.<br>• <b>Pico used before</b>: hold the <b>BOOTSEL</b> button while plugging in the cable, then '
        'release it.<br><br>Waiting for the Pico…',
    '2. Installazione del firmware': '2. Installing the firmware',
    'Ricerca del firmware più recente su GitHub…': 'Looking for the latest firmware on GitHub…',
    "GitHub non raggiungibile e nessun firmware incluso nell'app": 'GitHub not reachable and no firmware included in the app',
    '3. Avvio del ricevitore': '3. Starting the receiver',
    'Il Pico ha ricevuto il firmware e si sta riavviando come PS-RX (pochi secondi; la prima volta Windows prepara anche il dispositivo).':
        'The Pico received the firmware and is restarting as PS-RX (a few seconds; the first time Windows also '
        'sets up the device).',
    'Qualcosa non è andato': 'Something went wrong',
    'Riflashare il ricevitore?': 'Reflash the receiver?',
    'Il ricevitore si riavvia in modalità BOOTSEL e riceve di nuovo il firmware più recente. Impostazioni e abbinamenti restano. Servono i controller spenti.':
        'The receiver restarts in BOOTSEL mode and gets the latest firmware again. Settings and pairings are kept. '
        'The controllers must be off.',
    'Serve un Pico 2 W (con il WiFi/Bluetooth): il Pico 2 senza "W" non ha la radio. Tutto quello che c\'era sul Pico viene sostituito.':
        'You need a Pico 2 W (with WiFi/Bluetooth): the Pico 2 without "W" has no radio. Anything on the Pico is '
        'replaced.',
    "incluso nell'app, GitHub non raggiungibile": 'included in the app, GitHub not reachable',
    'Installazione non riuscita: {0}': 'Installation failed: {0}',
    '<br><br>Ricollega il Pico (tenendo BOOTSEL) per riprovare.': '<br><br>Plug the Pico in again (holding BOOTSEL) to retry.',
    'Ricevitore in modalità BOOTSEL': 'Receiver in BOOTSEL mode',
    'Copia di PS-RX {0} ({1}) nel Pico: non scollegarlo…': 'Copying PS-RX {0} ({1}) to the Pico: do not unplug it…',
    'versione {0} da GitHub': 'version {0} from GitHub',
    'Fatto: il ricevitore è pronto': 'Done: the receiver is ready',
    'PS-RX {0} è in funzione.<br>Ora abbina un controller: premi il pulsante qui sotto, poi tieni premuti <b>Create + PS</b> (DualSense) o <b>Share + PS</b> (DualShock 4) finché la luce lampeggia veloce.':
        'PS-RX {0} is running.<br>Now pair a controller: press the button below, then hold <b>Create + PS</b> '
        '(DualSense) or <b>Share + PS</b> (DualShock 4) until the light blinks quickly.',
    'Fine': 'Finish',
    'Il Pico non si è presentato come PS-RX. È un Pico 2 <b>W</b>? Prova a scollegarlo e ricollegarlo; se non parte, ripeti la procedura.':
        'The Pico did not show up as PS-RX. Is it a Pico 2 <b>W</b>? Try unplugging it and plugging it back in; '
        'if it does not start, repeat the procedure.',

    # --- app: scheda Gamepad ------------------------------------------------------------------------------
    'Spegni': 'Turn off',
    "Spegne il controller (l'abbinamento resta)": 'Turns the controller off (the pairing is kept)',
    '{0} · {1} report/s · {2} Hz · segnale {3}{4}': '{0} · {1} reports/s · {2} Hz · signal {3}{4}',
    'Spegni tutti': 'Turn all off',
    'Per abbinare: premi il pulsante qui accanto (o un click sul BOOTSEL del Pico), poi tieni premuti Create + PS (DualSense) o Share + PS (DualShock 4) finché la luce lampeggia veloce.':
        "To pair: press the button next to this (or click the Pico's BOOTSEL once), then hold Create + PS "
        '(DualSense) or Share + PS (DualShock 4) until the light blinks quickly.',
    'Controller abbinati': 'Paired controllers',
    'Nome (facoltativo)': 'Name (optional)',
    'Rinomina': 'Rename',
    'Dimentica questo controller': 'Forget this controller',
    'Tutti i controller': 'All controllers',
    " (il ricevitore si ricollega all'USB per cambiare forma)": ' (the receiver reconnects to USB to change its shape)',
    'Nome salvato: {0}': 'Name saved: {0}',
    '{0} dimenticato': '{0} forgotten',
    'Libero': 'Free',
    'Batteria %p%{0}': 'Battery %p%{0}',
    'Batteria sconosciuta': 'Battery unknown',
    'Abbinamento in corso…': 'Pairing…',
    '  ·  posto {0}': '  ·  slot {0}',
    'Dimenticare il controller?': 'Forget the controller?',
    '{0} va abbinato di nuovo per usarlo. Le sue impostazioni si perdono.':
        '{0} must be paired again to use it. Its settings are lost.',
    'Posto {0} spento': 'Slot {0} turned off',
    ' · audio': ' · audio',
    'Controller spenti': 'Controllers turned off',
    ' · in carica': ' · charging',

    # --- app: scheda Rete ---------------------------------------------------------------------------------
    '{0} h {1} min fa': '{0} h {1} min ago',
    '{0} s fa': '{0} s ago',
    '{0} fa': '{0} ago',
    'mai': 'never',
    '{0} min fa': '{0} min ago',
    '<br>Ultimo Wake-on-LAN: {0}': '<br>Last Wake-on-LAN: {0}',
    'Rete {0}': 'Network {0}',
    'Mostra': 'Show',
    'WPA3': 'WPA3',
    'Nome della rete (SSID)': 'Network name (SSID)',
    'Password': 'Password',
    'WiFi del ricevitore': "Receiver's WiFi",
    'Reti salvate (fino a {0})': 'Saved networks (up to {0})',
    'Aggiungi o modifica…': 'Add or edit…',
    'Elimina': 'Delete',
    'Prova la rete': 'Test the network',
    'Controlla password, indirizzo dal router e internet (solo senza controller collegati)':
        'Checks password, address from the router and internet (only with no controllers connected)',
    'Reti rilevate': 'Networks found',
    'Cerca reti': 'Scan networks',
    'Usa questa rete…': 'Use this network…',
    'Salva': 'Save',
    'Prova ora': 'Test now',
    'Manda subito il pacchetto (serve il WiFi connesso, quindi nessun controller)':
        'Sends the packet now (needs the WiFi connected, so no controllers)',
    'Connesso a <b>{0}</b> · IP {1} · segnale {2} dBm': 'Connected to <b>{0}</b> · IP {1} · signal {2} dBm',
    ' · invio del Wake-on-LAN in corso': ' · sending Wake-on-LAN',
    'Ricerca delle reti (qualche secondo)…': 'Scanning networks (a few seconds)…',
    'Rete {0} salvata: {1}': 'Network {0} saved: {1}',
    'Prova di "{0}": collegamento alla rete…': 'Testing "{0}": connecting to the network…',
    'Prova di "{0}": {1}': 'Testing "{0}": {1}',
    'Eliminare la rete?': 'Delete the network?',
    '{0} verrà tolta dal ricevitore.': '{0} will be removed from the receiver.',
    'Destinazioni del Wake-on-LAN salvate': 'Wake-on-LAN targets saved',
    'vuota = mantieni quella salvata': 'empty = keep the saved one',
    '<b>Solo reti a 2,4 GHz</b>: il Pico 2 W non vede le reti a 5 GHz. Se il router usa lo stesso nome per 2,4 e 5 GHz va bene, si collega da solo ai 2,4; se ha due nomi (per esempio "Casa" e "Casa_5G"), scegli quello a 2,4 GHz.':
        '<b>2.4 GHz networks only</b>: the Pico 2 W cannot see 5 GHz networks. If the router uses the same name '
        'for 2.4 and 5 GHz that is fine, it connects to 2.4 by itself; if it has two names (for example "Home" '
        'and "Home_5G"), pick the 2.4 GHz one.',
    'WPA2 va bene per quasi tutte le reti: WPA3 solo se il router lo richiede. Lascia la password vuota per una rete aperta. Dopo il salvataggio parte una prova.':
        'WPA2 is fine for almost every network: WPA3 only if the router requires it. Leave the password empty for '
        'an open network. A test starts after saving.',
    'Serve il nome della rete': 'The network name is required',
    "Il WiFi è acceso solo senza controller collegati, per mandare il Wake-on-LAN. Al primo controller il ricevitore manda 3 pacchetti (entro 30 s al massimo), poi spegne il WiFi: la radio resta tutta al Bluetooth. Si riaccende quando si spegne l'ultimo. Il Pico 2 W usa solo reti a <b>2,4 GHz</b>.":
        'The WiFi is on only with no controllers connected, to send Wake-on-LAN. With the first controller the '
        'receiver sends 3 packets (within 30 s at most), then turns the WiFi off: the radio is left entirely to '
        'Bluetooth. It turns back on when the last controller is turned off. The Pico 2 W only uses '
        '<b>2.4 GHz</b> networks.',
    'Nome della rete': 'Network name',
    'Sicurezza': 'Security',
    'Il ricevitore prova le reti in ordine e si collega alla prima che trova.':
        'The receiver tries the networks in order and connects to the first one it finds.',
    'Segnale': 'Signal',
    'Canale': 'Channel',
    'Le reti che il ricevitore vede da dove si trova (solo 2,4 GHz; le reti nascoste non compaiono). Doppio clic su una rete per salvarla. Solo senza controller collegati.':
        'The networks the receiver sees from where it is (2.4 GHz only; hidden networks do not appear). '
        'Double-click a network to save it. Only with no controllers connected.',
    'AA:BB:CC:DD:EE:FF': 'AA:BB:CC:DD:EE:FF',
    'Questo PC': 'This PC',
    'Il MAC è quello della scheda di rete cablata del PC da accendere, con il Wake-on-LAN attivo nel BIOS e in Windows. Perché il PC si accenda da spento, la porta USB del ricevitore deve restare alimentata a PC spento (opzione del BIOS tipo "USB power in S5" o "ErP" disattivato).':
        "The MAC is the wired network card's of the PC to turn on, with Wake-on-LAN enabled in the BIOS and in "
        "Windows. For the PC to turn on from off, the receiver's USB port must stay powered while the PC is off "
        '(a BIOS option like "USB power in S5", or "ErP" disabled).',
    'Spegni i controller: con un controller collegato il WiFi è spento':
        'Turn off the controllers: with a controller connected the WiFi is off',
    'Controlla password, indirizzo dal router e internet': 'Checks password, address from the router and internet',
    'Ricevitore non collegato': 'Receiver not connected',
    'Spento: ci sono controller collegati, la radio è tutta al Bluetooth':
        'Off: controllers are connected, the radio is entirely given to Bluetooth',
    'Ricerca non avviata: {0}': 'Scan not started: {0}',
    'La ricerca non ha dato risposta in tempo: riprova.': 'The scan did not answer in time: try again.',
    'Ricerca interrotta (si è collegato un controller?): riprova.': 'Scan interrupted (did a controller connect?): try again.',
    '{0} reti rilevate.': '{0} networks found.',
    'Nessuna rete rilevata: il router è acceso e trasmette a 2,4 GHz?':
        'No networks found: is the router on and broadcasting at 2.4 GHz?',
    'Già {0} reti salvate: eliminane una per aggiungere "{1}"': 'Already {0} saved networks: delete one to add "{1}"',
    'Scegli una rete salvata da provare': 'Pick a saved network to test',
    'Prova non avviata: {0}': 'Test not started: {0}',
    'La prova non ha dato risposta in tempo: riprova.': 'The test did not answer in time: try again.',
    'Rete {0} eliminata': 'Network {0} deleted',
    'rete aperta: nessuna password': 'open network: no password',
    'Nome troppo lungo (massimo 32 byte)': 'Name too long (32 bytes at most)',
    'Pacchetto Wake-on-LAN inviato': 'Wake-on-LAN packet sent',
    '(vuota)': '(empty)',
    '  (salvata)': '  (saved)',
    'La password WPA va da 8 a 63 caratteri': 'The WPA password must be 8 to 63 characters',
    'PC da svegliare {0}': 'PC to wake {0}',
    'Nessuna scheda di rete trovata': 'No network card found',
    'WPA2': 'WPA2',
    'salvata': 'saved',
    'nessuna': 'none',

    # --- app: scheda Sistema ------------------------------------------------------------------------------
    '{0} g {1} h': '{0} d {1} h',
    "{0} {1} sull'USB": '{0} {1} on USB',
    'Registro del ricevitore': 'Receiver log',
    'Aggiorna': 'Refresh',
    'Ricevitore': 'Receiver',
    'Impostazioni del ricevitore': 'Receiver settings',
    'Impostazioni di fabbrica': 'Factory settings',
    'Reti WiFi, Wake-on-LAN, abbinamenti e impostazioni dei controller restano':
        'WiFi networks, Wake-on-LAN, pairings and controller settings are kept',
    'Salva ora': 'Save now',
    'Le modifiche si salvano da sole appena non ci sono controller collegati':
        'Changes are saved by themselves as soon as no controllers are connected',
    'Firmware': 'Firmware',
    'Aggiorna il firmware…': 'Update the firmware…',
    'Riavvia in modalità aggiornamento (BOOTSEL)': 'Restart in update mode (BOOTSEL)',
    'Registro…': 'Log…',
    'Firmware su un Pico 2 W nuovo, o di nuovo su questo ricevitore':
        'Firmware on a new Pico 2 W, or again on this receiver',
    'Aggiornamenti (GitHub)': 'Updates (GitHub)',
    'Nessuna ricerca in questa sessione.': 'No check in this session.',
    'Cerca aggiornamenti': 'Check for updates',
    'Installa il firmware': 'Install the firmware',
    "Aggiorna l'app": 'Update the app',
    "Cerca aggiornamenti all'avvio (al massimo una volta al giorno)": 'Check for updates at startup (at most once a day)',
    'App': 'App',
    "Avvia con Windows (nell'area di notifica)": 'Start with Windows (in the notification area)',
    'Notifiche di collegamento e batteria': 'Connection and battery notifications',
    "Chiudendo la finestra l'app resta nell'area di notifica": 'When the window is closed the app stays in the notification area',
    ' (PC in sospensione)': ' (PC asleep)',
    '<b>PS-RX {0}</b> · base {1} · acceso da {2}': '<b>PS-RX {0}</b> · base {1} · up for {2}',
    'Modalità {0} · {1} · audio: {2}': '{0} mode · {1} · audio: {2}',
    'Impostazioni di fabbrica?': 'Factory settings?',
    'Le impostazioni del ricevitore tornano quelle iniziali (modalità PlayStation, posti dinamici...). Reti WiFi, Wake-on-LAN e controller abbinati restano.':
        'The receiver settings go back to the initial ones (PlayStation mode, dynamic slots...). WiFi networks, '
        'Wake-on-LAN and paired controllers are kept.',
    'Riavviare in modalità aggiornamento?': 'Restart in update mode?',
    'Il ricevitore si riavvia come chiavetta "RP2350": copia il file .uf2 al suo interno. Servono i controller spenti.':
        'The receiver restarts as an "RP2350" drive: copy the .uf2 file into it. The controllers must be off.',
    'Ultima versione: <b>{0}</b> · <a href="{1}">note della release</a>':
        'Latest version: <b>{0}</b> · <a href="{1}">release notes</a>',
    'ricevitore non collegato': 'receiver not connected',
    'Installa il firmware {0}': 'Install firmware {0}',
    "Aggiorna l'app a {0}": 'Update the app to {0}',
    'Download di {0}…': 'Downloading {0}…',
    'Annulla': 'Cancel',
    'Aggiornamento del firmware': 'Firmware update',
    "Aggiornamento dell'app": 'App update',
    "Aggiornare l'app?": 'Update the app?',
    "Windows chiede il permesso di installare; poi l'app si chiude e si riapre da sola con la versione nuova.":
        'Windows asks for permission to install; then the app closes and reopens by itself with the new version.',
    'Firmware PS-RX': 'PS-RX firmware',
    'Firmware (*.uf2 *.bin)': 'Firmware (*.uf2 *.bin)',
    'Preparazione…': 'Preparing…',
    'Spegni i controller: il firmware si carica e si installa solo senza controller collegati.':
        'Turn off the controllers: the firmware is uploaded and installed only with no controllers connected.',
    'Caricamento: blocco {0} di {1}': 'Uploading: block {0} of {1}',
    "Verifica dell'impronta SHA-256…": 'Checking the SHA-256 hash…',
    'Installazione: il ricevitore si riavvia (non staccarlo)…': 'Installing: the receiver restarts (do not unplug it)…',
    "L'aggiornamento dall'app carica il file (.uf2 o .bin), ne verifica l'impronta e lo installa: serve che i controller siano spenti. In modalità BOOTSEL il Pico compare come chiavetta e il file .uf2 si copia a mano.":
        'Updating from the app uploads the file (.uf2 or .bin), checks its hash and installs it: the controllers '
        'must be off. In BOOTSEL mode the Pico shows up as a drive and the .uf2 file is copied by hand.',
    'Automatica (lingua di Windows)': 'Automatic (Windows language)',
    'controller Xbox': 'Xbox controllers',
    'Memoria libera {0} KB': 'Free memory {0} KB',
    "La lingua cambia al riavvio dell'app. Riavviarla adesso?": 'The language changes when the app restarts. Restart it now?',
    "La lingua cambia al prossimo avvio dell'app": 'The language changes the next time the app starts',
    'Impostazioni di fabbrica ripristinate': 'Factory settings restored',
    'Ricerca su GitHub…': 'Checking GitHub…',
    'Ricerca non riuscita: {0}': 'Check failed: {0}',
    'Firmware del ricevitore: {0}': 'Receiver firmware: {0}',
    "L'app gira da sorgente: aggiornala dalla repo (git pull).": 'The app runs from source: update it from the repo (git pull).',
    'Download non riuscito: {0}': 'Download failed: {0}',
    'Aggiornare il firmware?': 'Update the firmware?',
    'Installata: {0}\nNuova: {1} ({2} KB)\n\nIl ricevitore si riavvia alla fine.':
        'Installed: {0}\nNew: {1} ({2} KB)\n\nThe receiver restarts at the end.',
    'Firmware installato: il ricevitore si riavvia': 'Firmware installed: the receiver restarts',
    'Registro spento: attiva "Registro diagnostico" qui sotto nella scheda Sistema.':
        'Log disabled: enable "Diagnostic log" below in the System tab.',
    'Impostazioni salvate': 'Settings saved',
    'Lingua / Language': 'Lingua / Language',
    ' · modifiche in attesa di salvataggio (a controller spenti)': ' · changes waiting to be saved (with controllers off)',
    ' → <b>aggiornamento disponibile</b>': ' → <b>update available</b>',
    'File non valido: {0}': 'Invalid file: {0}',
    'Aggiornamento annullato': 'Update cancelled',
    'Aggiornamento non riuscito: {0}': 'Update failed: {0}',
    "Aggiornamento dell'app non riuscito: {0}": 'App update failed: {0}',
    'sconosciuta': 'unknown',

    # --- schema delle impostazioni ------------------------------------------------------------------------
    'Modalità del ricevitore': 'Receiver mode',
    'Come il PC vede i controller. PlayStation: DualSense (anche il DualShock 4), con tutto. Xbox: controller Xbox 360 (XInput), senza giroscopio e audio; il touchpad funziona solo come mouse. Steam: ogni controller diventa un nuovo Steam Controller (2026), con giroscopio e il touchpad diviso nei due trackpad; funziona solo con Steam aperto.':
        'How the PC sees the controllers. PlayStation: DualSense (the DualShock 4 too), with everything. Xbox: '
        'Xbox 360 controller (XInput), without gyro and audio; the touchpad only works as a mouse. Steam: each '
        'controller becomes a new Steam Controller (2026), with gyro and the touchpad split into the two '
        'trackpads; it only works with Steam open.',
    'Steam Controller 2026 (sperimentale)': 'Steam Controller 2026 (experimental)',
    "Sempre 4 gamepad sull'USB": 'Always 4 gamepads on USB',
    'Attivo: il PC vede sempre 4 gamepad e collegare un controller non interrompe gli altri. Spento: il PC vede solo quelli accesi, ma quando se ne collega uno nuovo gli altri si fermano per circa 1 s.':
        'On: the PC always sees 4 gamepads and connecting a controller does not interrupt the others. Off: the PC '
        'only sees the ones turned on, but when a new one connects the others pause for about 1 s.',
    'Colore della barra luminosa secondo il posto': 'Light bar color by slot',
    'Come la PS5: 1 blu, 2 rosso, 3 verde, 4 rosa. Ignora i colori decisi dai giochi.':
        'Like the PS5: 1 blue, 2 red, 3 green, 4 pink. Ignores the colors set by games.',
    'Spegni i controller quando il PC si spegne o va in sospensione': 'Turn off the controllers when the PC shuts down or sleeps',
    "Il ricevitore capisce lo stato del PC dall'USB (Windows e Linux), senza WiFi. Per riaccenderlo basta premere PS: se il PC è sospeso e permette il risveglio via USB lo sveglia così, altrimenti (o a PC spento) usa il Wake-on-LAN. A PC acceso il Wake-on-LAN non parte e il WiFi si spegne subito. Serve che la porta USB resti alimentata a PC spento o sospeso.":
        "The receiver understands the PC's state from USB (Windows and Linux), without WiFi. To turn it back on "
        'just press PS: if the PC is asleep and allows USB wake-up it wakes it that way, otherwise (or with the PC '
        'off) it uses Wake-on-LAN. With the PC on, Wake-on-LAN is not sent and the WiFi turns off right away. The '
        'USB port must stay powered while the PC is off or asleep.',
    'Disattiva il Wake-on-LAN': 'Disable Wake-on-LAN',
    'Senza Wake-on-LAN il WiFi si spegne appena si collega un controller.':
        'Without Wake-on-LAN the WiFi turns off as soon as a controller connects.',
    'Buffer audio del controller': 'Controller audio buffer',
    "Più alto = meno scatti in cuffia, con un po' di ritardo solo sull'audio. Non tocca i tasti.":
        'Higher = fewer glitches in the headset, with a little delay on the audio only. It does not affect buttons.',
    'Spegni il controller dopo (minuti di inattività)': 'Turn the controller off after (minutes of inactivity)',
    'Il controller si spegne se resta fermo per questo tempo.': 'The controller turns off if left idle for this long.',
    'Non spegnere mai i controller per inattività': 'Never turn off controllers for inactivity',
    'LED del ricevitore spento': 'Receiver LED off',
    "Di solito il LED del Pico resta acceso finché c'è almeno un controller collegato.":
        "Normally the Pico's LED stays on while at least one controller is connected.",
    'Sveglia il PC dalla sospensione via USB': 'Wake the PC from sleep over USB',
    'Aggiunge una piccola tastiera USB che preme un tasto quando premi PS a PC sospeso: è il modo più affidabile per svegliare il PC dalla sospensione, perché Windows lascia sempre svegliare il PC da una tastiera. Alcuni anticheat notano la tastiera: se ti dà problemi spegnila (resta il Wake-on-LAN).':
        'Adds a small USB keyboard that presses a key when you press PS while the PC sleeps: it is the most '
        'reliable way to wake the PC from sleep, because Windows always lets a keyboard wake the PC. Some '
        'anti-cheat systems notice the keyboard: if it causes problems turn it off (Wake-on-LAN remains).',
    'Registro diagnostico': 'Diagnostic log',
    "Tiene in RAM le ultime righe di log del ricevitore, da leggere nell'app.":
        "Keeps the receiver's last log lines in RAM, to read in the app.",
    'Audio (altoparlante e cuffie del controller)': "Audio (controller's speaker and headset)",
    "Solo con un controller collegato: con due o più il Bluetooth non regge l'audio. Spento: Windows non mostra più altoparlante e microfono del controller (il ricevitore si ricollega all'USB, circa 1 s) e il Bluetooth ha più banda. Non disponibile sul DualShock 4.":
        "Only with one controller connected: with two or more, Bluetooth cannot carry the audio. Off: Windows no "
        "longer shows the controller's speaker and microphone (the receiver reconnects to USB, about 1 s) and "
        'Bluetooth has more bandwidth. Not available on the DualShock 4.',
    'Microfono (integrato o delle cuffie)': 'Microphone (built-in or headset)',
    "Trasmette solo quando un'app usa il microfono. Richiede l'audio attivo.":
        'Transmits only when an app uses the microphone. Requires audio on.',
    'Frequenza verso il PC': 'Rate to the PC',
    'Quante volte al secondo il PC riceve lo stato di questo controller. 1000 Hz = latenza minima.':
        "How many times per second the PC receives this controller's state. 1000 Hz = lowest latency.",
    '1000 Hz': '1000 Hz',
    '500 Hz': '500 Hz',
    '250 Hz': '250 Hz',
    '125 Hz': '125 Hz',
    'Touchpad come mouse': 'Touchpad as mouse',
    'Il touchpad muove il puntatore e il suo click è il tasto sinistro; la striscia a sinistra fa da rotellina.':
        'The touchpad moves the pointer and its click is the left button; the left strip works as a scroll wheel.',
    'Inverti la rotellina': 'Invert the scroll wheel',
}
