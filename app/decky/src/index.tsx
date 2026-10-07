// PS-RX - pannello del plugin Decky (menu rapido di Steam, usabile col controller).
//
// Tre sezioni come l'app Windows: Gamepad (posti, controller abbinati e loro impostazioni), Rete (WiFi
// del ricevitore, reti salvate, Wake-on-LAN) e Sistema (impostazioni del ricevitore, firmware, registro).
// Lo stato si chiede ogni 2 s solo finche' il pannello e' aperto; le notifiche le manda il backend.

import {
  ButtonItem,
  ConfirmModal,
  DropdownItem,
  Field,
  ModalRoot,
  PanelSection,
  PanelSectionRow,
  ProgressBarWithInfo,
  SliderField,
  TextField,
  ToggleField,
  showModal,
  staticClasses,
} from "@decky/ui";
import {
  FileSelectionType,
  addEventListener,
  callable,
  definePlugin,
  openFilePicker,
  removeEventListener,
  toaster,
} from "@decky/api";
import { useEffect, useRef, useState } from "react";
import { FaGamepad } from "react-icons/fa";

// --- tipi (risposte del backend main.py) ---------------------------------------------
interface Errore { errore?: string; codice?: number }
interface Pad {
  posto: number; modello: string; mac: string; batteria: number; batteria_valida: boolean;
  in_carica: boolean; rssi: number | null; report: number; hz: number; audio: boolean;
}
interface Stato extends Errore {
  versione: string; base: string; modalita: number; nome_modalita: string; usb_gamepad: number;
  usb_sospeso: boolean; posti_fissi: boolean; finestra_abbinamento: boolean; altoparlante: boolean;
  microfono: boolean; microfono_muto: boolean; salvataggio_in_sospeso: boolean; pad_connessi: number;
  uptime_s: number; heap_libero: number; rete: string; rete_connessa: boolean; rete_ip: string;
  rete_rssi: number; ms_da_ultimo_wol: number | null; pad: Pad[]; caricamento: number;
  caricamento_testo: string; caricatore_attivo: boolean;
}
interface Voce {
  id: number; chiave: string; tipo: "scelta" | "intero" | "booleano"; titolo: string; nota: string;
  sezione?: string; riconnette?: boolean; opzioni?: [number, string][]; min?: number; max?: number; passo?: number;
}
interface Impostazioni extends Errore { schema: Voce[]; valori: Record<string, number> }
interface Abbinato { mac: string; nome: string; posto: number | null; valori: Record<string, number> }
interface Abbinati extends Errore { schema?: Voce[]; abbinati?: Abbinato[] }
interface Rete { indice: number; ssid: string; wpa3: boolean; ha_password: boolean }
interface Reti extends Errore { reti?: Rete[]; wol_mac?: string[] }
interface Firmware extends Errore { nome: string; versione: string; dimensione: number }
interface Avanzamento { fase: string; fatto: number; totale: number; messaggio: string }

// --- chiamate al backend ---------------------------------------------------------------
const leggiStato = callable<[], Stato>("stato");
const leggiImpostazioni = callable<[], Impostazioni>("impostazioni");
const leggiAbbinati = callable<[], Abbinati>("abbinati");
const leggiReti = callable<[], Reti>("reti");
const leggiRegistro = callable<[], { attivo?: boolean; testo?: string } & Errore>("registro");
const imposta = callable<[number, number], Errore>("imposta");
const impostaPad = callable<[string, number, number], Errore>("imposta_pad");
const salvaOra = callable<[], Errore>("salva_ora");
const predefinite = callable<[], Errore>("predefinite");
const rinomina = callable<[string, string], Errore>("rinomina");
const dimentica = callable<[string], Errore>("dimentica");
const abbina = callable<[], Errore>("abbina");
const spegniPad = callable<[number], Errore>("spegni_pad");
const salvaRete = callable<[number, string, string, boolean, boolean], Errore>("salva_rete");
const cancellaRete = callable<[number], Errore>("cancella_rete");
const destinazioniWol = callable<[string, string], Errore>("destinazioni_wol");
const provaWol = callable<[], Errore>("prova_wol");
const bootsel = callable<[], Errore>("bootsel");
const preparaFirmware = callable<[string], Firmware>("prepara_firmware");
const caricaFirmware = callable<[], Errore>("carica_firmware");
const annullaCaricamento = callable<[], Errore>("annulla_caricamento");
const leggiNotifiche = callable<[], boolean>("notifiche");
interface Aggiornamenti extends Errore {
  versione?: string; pagina?: string; note?: string; firmware_installato?: string; firmware_nuovo?: boolean;
  plugin_installato?: string; plugin_nuovo?: boolean;
}
const cercaAggiornamenti = callable<[], Aggiornamenti>("aggiornamenti");
const installaFirmwareGithub = callable<[], Errore>("installa_firmware_github");
const aggiornaPlugin = callable<[], Errore>("aggiorna_plugin");
const leggiCercaAggiornamenti = callable<[], boolean>("cerca_aggiornamenti_attivo");
const impostaCercaAggiornamenti = callable<[boolean], Errore>("imposta_cerca_aggiornamenti");
const impostaNotifiche = callable<[boolean], Errore>("imposta_notifiche");

function avviso(titolo: string, testo: string) {
  toaster.toast({ title: titolo, body: testo });
}

// Esegue un'azione e mostra l'errore, se c'e'. Restituisce true se e' andata bene.
async function esegui(azione: Promise<Errore>, ok?: string): Promise<boolean> {
  const r = await azione;
  if (r.errore) {
    avviso("PS-RX", r.errore);
    return false;
  }
  if (ok) avviso("PS-RX", ok);
  return true;
}

function durata(s: number): string {
  if (s >= 3600) return `${Math.floor(s / 3600)} h ${Math.floor((s % 3600) / 60)} min`;
  return s >= 60 ? `${Math.floor(s / 60)} min` : `${s} s`;
}

// --- controllo generico costruito dallo schema -------------------------------------------------
function ControlloVoce(props: { voce: Voce; valore: number; cambia: (v: number) => void }) {
  const { voce: v, valore, cambia } = props;
  const nota = v.riconnette ? `${v.nota} (Ricollega l'USB del ricevitore.)` : v.nota;
  if (v.tipo === "booleano") {
    return (
      <PanelSectionRow>
        <ToggleField label={v.titolo} description={nota} checked={!!valore} onChange={(on) => cambia(on ? 1 : 0)} />
      </PanelSectionRow>
    );
  }
  if (v.tipo === "scelta") {
    return (
      <PanelSectionRow>
        <DropdownItem label={v.titolo} description={nota} selectedOption={valore}
          rgOptions={(v.opzioni ?? []).map(([data, label]) => ({ data, label }))}
          onChange={(o) => cambia(o.data as number)} />
      </PanelSectionRow>
    );
  }
  return (
    <PanelSectionRow>
      <SliderField label={v.titolo} description={nota} value={valore} min={v.min} max={v.max} step={v.passo}
        showValue onChange={(x) => cambia(x)} />
    </PanelSectionRow>
  );
}

// Impostazioni globali di una sezione ("gamepad", "rete", "sistema").
function ImpostazioniSezione(props: { sezione: string }) {
  const [imp, setImp] = useState<Impostazioni | null>(null);
  const attese = useRef<Record<number, number>>({});
  const carica = async () => { const r = await leggiImpostazioni(); if (!r.errore) setImp(r); };
  useEffect(() => { carica(); }, []);
  if (!imp) return null;
  const cambia = (voce: Voce, valore: number) => {
    setImp({ ...imp, valori: { ...imp.valori, [voce.id]: valore } });
    // i cursori mandano un valore a ogni passo: aspetto che si fermino
    window.clearTimeout(attese.current[voce.id]);
    attese.current[voce.id] = window.setTimeout(async () => {
      if (!(await esegui(imposta(voce.id, valore)))) carica();
    }, voce.tipo === "intero" ? 500 : 0);
  };
  return (
    <>
      {imp.schema.filter((v) => v.sezione === props.sezione).map((v) => (
        <ControlloVoce key={v.id} voce={v} valore={imp.valori[v.id] ?? 0} cambia={(x) => cambia(v, x)} />
      ))}
    </>
  );
}

// --- finestre di dialogo ----------------------------------------------------------------
function ModaleController(props: { ab: Abbinato; schema: Voce[]; closeModal?: () => void; fatto: () => void }) {
  const { ab, schema } = props;
  const [nome, setNome] = useState(ab.nome);
  const [valori, setValori] = useState(ab.valori);
  const cambia = async (v: Voce, x: number) => {
    setValori({ ...valori, [v.id]: x });
    if (!(await esegui(impostaPad(ab.mac, v.id, x)))) setValori(valori);
    props.fatto();
  };
  return (
    <ModalRoot closeModal={props.closeModal}>
      <div className={staticClasses.Title}>{ab.nome || ab.mac}</div>
      <PanelSectionRow>
        <TextField label="Nome (al massimo 15 caratteri)" value={nome} onChange={(e) => setNome(e.target.value.slice(0, 15))} />
      </PanelSectionRow>
      <PanelSectionRow>
        <ButtonItem layout="below" onClick={async () => { await esegui(rinomina(ab.mac, nome), "Nome salvato."); props.fatto(); }}>
          Salva il nome
        </ButtonItem>
      </PanelSectionRow>
      {schema.map((v) => (
        <ControlloVoce key={v.id} voce={v} valore={valori[v.id] ?? 0} cambia={(x) => cambia(v, x)} />
      ))}
      <PanelSectionRow>
        <ButtonItem layout="below" onClick={() => showModal(
          <ConfirmModal strTitle="Dimenticare il controller?"
            strDescription={`${ab.nome || ab.mac}: per riusarlo andrà abbinato di nuovo e le sue impostazioni si perdono.`}
            strOKButtonText="Dimentica" strCancelButtonText="Annulla"
            onOK={async () => { await esegui(dimentica(ab.mac), "Controller dimenticato."); props.fatto(); props.closeModal?.(); }} />)}>
          Dimentica questo controller
        </ButtonItem>
      </PanelSectionRow>
    </ModalRoot>
  );
}

function ModaleRete(props: { rete: Rete; closeModal?: () => void; fatto: () => void }) {
  const { rete } = props;
  const [ssid, setSsid] = useState(rete.ssid);
  const [pw, setPw] = useState("");
  const [wpa3, setWpa3] = useState(rete.wpa3);
  return (
    <ConfirmModal strTitle={`Rete ${rete.indice + 1}`} strOKButtonText="Salva" strCancelButtonText="Annulla"
      closeModal={props.closeModal}
      onOK={async () => {
        const mantieni = !pw && ssid === rete.ssid && rete.ha_password;
        await esegui(salvaRete(rete.indice, ssid, pw, wpa3, mantieni), `Rete ${ssid} salvata.`);
        props.fatto();
      }}>
      <TextField label="Nome della rete (SSID)" value={ssid} onChange={(e) => setSsid(e.target.value.slice(0, 32))} />
      <TextField label={rete.ha_password ? "Password (vuota = mantieni quella salvata)" : "Password (vuota = rete aperta)"}
        bIsPassword value={pw} onChange={(e) => setPw(e.target.value.slice(0, 63))} />
      <ToggleField label="WPA3" description="Solo se il router lo richiede: WPA2 va bene per quasi tutte le reti."
        checked={wpa3} onChange={setWpa3} />
    </ConfirmModal>
  );
}

function ModaleWol(props: { mac: string[]; closeModal?: () => void; fatto: () => void }) {
  const [m1, setM1] = useState(props.mac[0] ?? "");
  const [m2, setM2] = useState(props.mac[1] ?? "");
  return (
    <ConfirmModal strTitle="PC da svegliare" strOKButtonText="Salva" strCancelButtonText="Annulla"
      strDescription="MAC della scheda di rete cablata del PC (formato AA:BB:CC:DD:EE:FF). Vuoto = nessuno."
      closeModal={props.closeModal}
      onOK={async () => { await esegui(destinazioniWol(m1.trim(), m2.trim()), "Destinazioni salvate."); props.fatto(); }}>
      <TextField label="PC 1" value={m1} onChange={(e) => setM1(e.target.value.slice(0, 17))} />
      <TextField label="PC 2" value={m2} onChange={(e) => setM2(e.target.value.slice(0, 17))} />
    </ConfirmModal>
  );
}

function ModaleRegistro(props: { testo: string; closeModal?: () => void }) {
  return (
    <ModalRoot closeModal={props.closeModal}>
      <div style={{ fontFamily: "monospace", fontSize: "11px", whiteSpace: "pre-wrap", maxHeight: "60vh", overflowY: "auto" }}>
        {props.testo}
      </div>
    </ModalRoot>
  );
}

// --- sezioni ---------------------------------------------------------------------------------
function SezioneGamepad(props: { st: Stato }) {
  const { st } = props;
  const [ab, setAb] = useState<Abbinati | null>(null);
  const carica = async () => { const r = await leggiAbbinati(); if (!r.errore) setAb(r); };
  useEffect(() => { carica(); }, [st.pad_connessi]);
  const nomi: Record<string, string> = {};
  (ab?.abbinati ?? []).forEach((a) => { if (a.nome) nomi[a.mac] = a.nome; });
  return (
    <PanelSection title={`Gamepad · modalità ${st.nome_modalita}`}>
      {st.pad.length === 0 && (
        <PanelSectionRow><Field label="Controller" description="nessuno collegato" /></PanelSectionRow>
      )}
      {st.pad.map((pad) => (
        <PanelSectionRow key={pad.posto}>
          <ButtonItem layout="below"
            label={`${pad.posto + 1}. ${nomi[pad.mac] || pad.modello}: batteria ${pad.batteria_valida ? `${pad.batteria}%` : "?"}` +
              `${pad.in_carica ? " (in carica)" : ""}`}
            description={`${pad.modello} · ${pad.report} report/s · ${pad.hz} Hz · segnale ${pad.rssi ?? "-"}`}
            onClick={() => esegui(spegniPad(pad.posto), `Posto ${pad.posto + 1} spento.`)}>
            Spegni
          </ButtonItem>
        </PanelSectionRow>
      ))}
      <PanelSectionRow>
        <ButtonItem layout="below"
          description={st.finestra_abbinamento ? "Abbinamento in corso: tieni Create + PS (DualSense) o Share + PS (DualShock 4)."
            : "Finestra di 30 s: poi tieni Create + PS (DualSense) o Share + PS (DualShock 4) sul controller."}
          onClick={() => esegui(abbina(), "Abbinamento aperto per 30 secondi.")}>
          Abbina un nuovo controller
        </ButtonItem>
      </PanelSectionRow>
      {(ab?.abbinati ?? []).map((a) => (
        <PanelSectionRow key={a.mac}>
          <ButtonItem layout="below" label={a.nome || a.mac}
            description={`${a.mac} · ${a.posto !== null ? `posto ${a.posto + 1}` : "non collegato"}`}
            onClick={() => showModal(<ModaleController ab={a} schema={ab?.schema ?? []} fatto={carica} />)}>
            Impostazioni
          </ButtonItem>
        </PanelSectionRow>
      ))}
      <ImpostazioniSezione sezione="gamepad" />
    </PanelSection>
  );
}

function SezioneRete(props: { st: Stato }) {
  const { st } = props;
  const [reti, setReti] = useState<Reti | null>(null);
  const carica = async () => { const r = await leggiReti(); if (!r.errore) setReti(r); };
  useEffect(() => { carica(); }, []);
  const wol = st.ms_da_ultimo_wol === null ? "mai" : `${durata(Math.floor(st.ms_da_ultimo_wol / 1000))} fa`;
  return (
    <PanelSection title="Rete">
      <PanelSectionRow>
        <Field label="WiFi del ricevitore"
          description={st.rete_connessa ? `connesso · IP ${st.rete_ip} · segnale ${st.rete_rssi} dBm` :
            (st.pad_connessi ? "spento: con i controller collegati la radio è tutta al Bluetooth" : st.rete)} />
      </PanelSectionRow>
      <PanelSectionRow><Field label="Ultimo Wake-on-LAN" description={wol} /></PanelSectionRow>
      {(reti?.reti ?? []).map((r) => (
        <PanelSectionRow key={r.indice}>
          <ButtonItem layout="below" label={r.ssid ? `${r.indice + 1}. ${r.ssid}` : `${r.indice + 1}. (vuota)`}
            description={r.ssid ? `${r.wpa3 ? "WPA3" : "WPA2"} · password ${r.ha_password ? "salvata" : "nessuna"}` : ""}
            onClick={() => showModal(<ModaleRete rete={r} fatto={carica} />)}>
            {r.ssid ? "Modifica" : "Aggiungi"}
          </ButtonItem>
          {r.ssid && (
            <ButtonItem layout="below" onClick={async () => { await esegui(cancellaRete(r.indice), `Rete ${r.ssid} eliminata.`); carica(); }}>
              Elimina
            </ButtonItem>
          )}
        </PanelSectionRow>
      ))}
      <PanelSectionRow>
        <ButtonItem layout="below" label="PC da svegliare"
          description={(reti?.wol_mac ?? []).filter(Boolean).join(", ") || "nessuno"}
          onClick={() => showModal(<ModaleWol mac={reti?.wol_mac ?? []} fatto={carica} />)}>
          Modifica
        </ButtonItem>
      </PanelSectionRow>
      <PanelSectionRow>
        <ButtonItem layout="below" description="Serve il WiFi connesso, quindi nessun controller collegato."
          onClick={() => esegui(provaWol(), "Pacchetto Wake-on-LAN inviato.")}>
          Prova il Wake-on-LAN
        </ButtonItem>
      </PanelSectionRow>
      <ImpostazioniSezione sezione="rete" />
    </PanelSection>
  );
}

function SezioneFirmware(props: { st: Stato }) {
  const { st } = props;
  const [fw, setFw] = useState<Firmware | null>(null);
  const [av, setAv] = useState<Avanzamento | null>(null);
  useEffect(() => {
    const ascolta = addEventListener<[string, number, number, string]>("psrx_avanzamento",
      (fase, fatto, totale, messaggio) => {
        setAv({ fase, fatto, totale, messaggio });
        if (fase === "fine" || fase === "errore") avviso("PS-RX", messaggio);
      });
    return () => { removeEventListener("psrx_avanzamento", ascolta); };
  }, []);
  const scegli = async () => {
    try {
      const r = await openFilePicker(FileSelectionType.FILE, "/home/deck/Downloads", true, true, undefined, ["uf2", "bin"]);
      const info = await preparaFirmware(r.realpath || r.path);
      if (info.errore) { avviso("PS-RX", info.errore); setFw(null); } else setFw(info);
    } catch {
      // scelta annullata
    }
  };
  const attivo = st.caricatore_attivo;
  let testo = "";
  if (av && attivo) {
    if (av.fase === "attesa_pad") testo = "Spegni i controller: il firmware si carica e si installa solo senza controller collegati.";
    else if (av.fase === "caricamento") testo = `Caricamento: ${av.fatto} di ${av.totale} blocchi`;
    else if (av.fase === "verifica") testo = "Il ricevitore verifica il firmware (SHA-256)...";
    else if (av.fase === "installazione") testo = "Installazione: il ricevitore si riavvia (non staccarlo).";
  } else if (st.caricamento_testo) {
    testo = st.caricamento_testo;
  }
  return (
    <>
      <PanelSectionRow>
        <ButtonItem layout="below" disabled={attivo} onClick={scegli}
          description={fw ? `${fw.nome}: versione ${fw.versione}, ${Math.round(fw.dimensione / 1024)} KB` : `installata: ${st.versione}`}>
          Scegli il firmware (.uf2 o .bin)...
        </ButtonItem>
      </PanelSectionRow>
      <PanelSectionRow>
        {attivo ? (
          <ButtonItem layout="below" onClick={() => annullaCaricamento()}>Annulla l'aggiornamento</ButtonItem>
        ) : (
          <ButtonItem layout="below" disabled={!fw} description="Carica, verifica e installa: servono i controller spenti."
            onClick={() => { setAv(null); esegui(caricaFirmware()); }}>
            Aggiorna il ricevitore
          </ButtonItem>
        )}
      </PanelSectionRow>
      {attivo && av?.fase === "caricamento" && (
        <PanelSectionRow>
          <ProgressBarWithInfo nProgress={av.totale ? (100 * av.fatto) / av.totale : 0} sOperationText={testo} />
        </PanelSectionRow>
      )}
      {testo && !(attivo && av?.fase === "caricamento") && (
        <PanelSectionRow><Field label="Aggiornamento" description={testo} /></PanelSectionRow>
      )}
    </>
  );
}

function SezioneAggiornamenti() {
  const [agg, setAgg] = useState<Aggiornamenti | null>(null);
  const [cercando, setCercando] = useState(false);
  const [automatico, setAutomatico] = useState<boolean | null>(null);
  useEffect(() => { leggiCercaAggiornamenti().then(setAutomatico); }, []);
  const cerca = async () => {
    setCercando(true);
    const r = await cercaAggiornamenti();
    setCercando(false);
    if (r.errore) avviso("PS-RX", r.errore); else setAgg(r);
  };
  return (
    <>
      <PanelSectionRow>
        <ButtonItem layout="below" disabled={cercando} onClick={cerca}
          description={agg && !agg.errore ? `Ultima versione ${agg.versione}: firmware ${agg.firmware_nuovo ? "da aggiornare" : "aggiornato"}` +
            ` (${agg.firmware_installato || "ricevitore non collegato"}), plugin ${agg.plugin_nuovo ? "da aggiornare" : "aggiornato"} (${agg.plugin_installato}).`
            : "Controlla le release su GitHub."}>
          {cercando ? "Ricerca..." : "Cerca aggiornamenti"}
        </ButtonItem>
      </PanelSectionRow>
      {agg?.firmware_nuovo && (
        <PanelSectionRow>
          <ButtonItem layout="below" description="Scarica da GitHub e installa: servono i controller spenti."
            onClick={() => esegui(installaFirmwareGithub(), "Download del firmware avviato.")}>
            Installa il firmware {agg.versione}
          </ButtonItem>
        </PanelSectionRow>
      )}
      {agg?.plugin_nuovo && (
        <PanelSectionRow>
          <ButtonItem layout="below" description="Scarica il plugin nuovo e riavvia Decky Loader (pochi secondi)."
            onClick={() => esegui(aggiornaPlugin(), "Plugin aggiornato: Decky Loader si riavvia.")}>
            Aggiorna il plugin a {agg.versione}
          </ButtonItem>
        </PanelSectionRow>
      )}
      {automatico !== null && (
        <PanelSectionRow>
          <ToggleField label="Cerca aggiornamenti all'avvio" description="Al massimo una volta al giorno, con una notifica."
            checked={automatico} onChange={async (on) => { setAutomatico(on); await impostaCercaAggiornamenti(on); }} />
        </PanelSectionRow>
      )}
    </>
  );
}

function SezioneSistema(props: { st: Stato }) {
  const { st } = props;
  const [notifiche, setNotifiche] = useState<boolean | null>(null);
  useEffect(() => { leggiNotifiche().then(setNotifiche); }, []);
  const audio = [st.altoparlante ? "altoparlante" : "", st.microfono ? "microfono" : ""].filter(Boolean);
  return (
    <PanelSection title="Sistema">
      <PanelSectionRow>
        <Field label={`PS-RX ${st.versione}`}
          description={`acceso da ${durata(st.uptime_s)} · ${st.usb_gamepad} sull'USB · audio: ${audio.join(", ") || "non in uso"}` +
            `${st.salvataggio_in_sospeso ? " · modifiche in attesa di salvataggio (a controller spenti)" : ""}`} />
      </PanelSectionRow>
      {notifiche !== null && (
        <PanelSectionRow>
          <ToggleField label="Notifiche" description="Controller collegato (posto, modello, modalità, batteria) e batteria in esaurimento."
            checked={notifiche} onChange={async (on) => { setNotifiche(on); await impostaNotifiche(on); }} />
        </PanelSectionRow>
      )}
      <ImpostazioniSezione sezione="sistema" />
      <SezioneFirmware st={st} />
      <SezioneAggiornamenti />
      <PanelSectionRow>
        <ButtonItem layout="below" disabled={st.pad_connessi > 0}
          description="Il ricevitore si riavvia come chiavetta RP2350 (solo senza controller)."
          onClick={() => esegui(bootsel(), "Ricevitore in modalità BOOTSEL.")}>
          Modalità aggiornamento (BOOTSEL)
        </ButtonItem>
      </PanelSectionRow>
      <PanelSectionRow>
        <ButtonItem layout="below" description="Di solito non serve: le modifiche si salvano da sole a controller spenti."
          onClick={() => esegui(salvaOra(), "Impostazioni salvate.")}>
          Salva ora
        </ButtonItem>
      </PanelSectionRow>
      <PanelSectionRow>
        <ButtonItem layout="below" onClick={() => showModal(
          <ConfirmModal strTitle="Impostazioni di fabbrica"
            strDescription="Reti WiFi, Wake-on-LAN, controller abbinati e loro impostazioni restano."
            strOKButtonText="Ripristina" strCancelButtonText="Annulla"
            onOK={() => esegui(predefinite(), "Impostazioni di fabbrica ripristinate.")} />)}>
          Impostazioni di fabbrica
        </ButtonItem>
      </PanelSectionRow>
      <PanelSectionRow>
        <ButtonItem layout="below" onClick={async () => {
          const r = await leggiRegistro();
          const testo = r.errore ?? (r.attivo ? r.testo ?? "" : "Registro spento: attiva \"Registro diagnostico\".");
          showModal(<ModaleRegistro testo={testo} />);
        }}>
          Mostra il registro
        </ButtonItem>
      </PanelSectionRow>
    </PanelSection>
  );
}

function Contenuto() {
  const [st, setSt] = useState<Stato | null>(null);
  const [errore, setErrore] = useState("");
  useEffect(() => {
    let vivo = true;
    const aggiorna = async () => {
      const r = await leggiStato();
      if (!vivo) return;
      if (r.errore) { setErrore(r.errore); setSt(null); } else { setErrore(""); setSt(r); }
    };
    aggiorna();
    const t = window.setInterval(aggiorna, 2000);
    return () => { vivo = false; window.clearInterval(t); };
  }, []);
  if (!st) {
    return (
      <PanelSection title="PS-RX">
        <PanelSectionRow><Field label="Ricevitore" description={errore || "ricerca..."} /></PanelSectionRow>
      </PanelSection>
    );
  }
  return (
    <>
      <SezioneGamepad st={st} />
      <SezioneRete st={st} />
      <SezioneSistema st={st} />
    </>
  );
}

export default definePlugin(() => {
  // Registrato al caricamento del plugin, non nel pannello: le notifiche arrivano anche a menu chiuso.
  const notifica = addEventListener<[string, string, boolean]>("psrx_notifica", (titolo, testo, critica) => {
    toaster.toast({ title: titolo, body: testo, critical: critica, playSound: critica });
  });
  return {
    name: "PS-RX",
    titleView: <div className={staticClasses.Title}>PS-RX</div>,
    content: <Contenuto />,
    icon: <FaGamepad />,
    onDismount() { removeEventListener("psrx_notifica", notifica); },
  };
});
