; PS-RX - installer per Windows (Inno Setup 6). Lo compila strumenti\compila_installer.ps1:
;   ISCC.exe /DVersione=X.Y.Z /DExe=...\PS-RX.exe /DIcona=...\ps-rx.ico /O<cartella> ps-rx.iss
;
; Installa per tutti gli utenti in Programmi\PS-RX, con la voce nel menu Start, l'icona facoltativa sul
; desktop e la disinstallazione in "App installate". L'app si aggiorna lanciando lo stesso installer in
; modo silenzioso (/SILENT): chiude l'app aperta, sostituisce i file e la riapre.

#ifndef Versione
  #define Versione "0.0.0"
#endif

[Setup]
AppId={{8E1B2C7A-4F63-4D2E-9A51-3C7B6E0F2D94}
AppName=PS-RX
AppVersion={#Versione}
AppVerName=PS-RX {#Versione}
AppPublisher=PS-RX
AppPublisherURL=https://github.com/loddi-o/PS-RX
AppSupportURL=https://github.com/loddi-o/PS-RX/issues
AppUpdatesURL=https://github.com/loddi-o/PS-RX/releases
DefaultDirName={autopf}\PS-RX
DefaultGroupName=PS-RX
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=PS-RX-Setup-{#Versione}
SetupIconFile={#Icona}
UninstallDisplayIcon={app}\PS-RX.exe
UninstallDisplayName=PS-RX
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=force
RestartApplications=no
VersionInfoVersion={#Versione}
VersionInfoProductName=PS-RX

[Languages]
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"

[Tasks]
Name: "desktop"; Description: "Icona sul desktop"; GroupDescription: "Collegamenti:"; Flags: unchecked

[Files]
Source: "{#Exe}"; DestDir: "{app}"; DestName: "PS-RX.exe"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\PS-RX"; Filename: "{app}\PS-RX.exe"; Comment: "Impostazioni e notifiche del ricevitore PS-RX"
Name: "{autodesktop}\PS-RX"; Filename: "{app}\PS-RX.exe"; Tasks: desktop

[Registry]
; L'app scrive qui l'avvio con Windows (scheda Sistema): la disinstallazione lo toglie.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "PS-RX"; Flags: uninsdeletevalue dontcreatekey

[Run]
; Installazione normale: casella "Avvia PS-RX" alla fine. Aggiornamento silenzioso: l'app si riapre da sola.
Filename: "{app}\PS-RX.exe"; Description: "Avvia PS-RX"; Flags: postinstall nowait skipifsilent
Filename: "{app}\PS-RX.exe"; Flags: nowait runasoriginaluser; Check: WizardSilent
