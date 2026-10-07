#ifndef DS5_BRIDGE_WEB_PAGE_H
#define DS5_BRIDGE_WEB_PAGE_H

// Config UI served over WiFi at http://<hostname>.local/ (default ds5.local).
// Single self-contained page; loads from GET /api/config and persists via
// POST /api/config. Settings mirror Config_body (src/config.h); the firmware
// re-validates every field, so the page is a convenience, not the source of
// truth for bounds.
static const char WEB_PAGE[] = R"rawhtml(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>DS5-Linux-Bridge</title>
<style>
:root{color-scheme:dark}
body{font-family:system-ui,sans-serif;background:#111;color:#eee;max-width:560px;margin:2rem auto;padding:0 1rem}
h1{font-size:1.4rem}h1 small{color:#888;font-weight:normal;font-size:.7em}
.field{margin:1.1rem 0}
label.lbl{display:block;margin-bottom:.3rem;font-size:.95rem}
.hint{color:#888;font-size:.8rem;margin-top:.2rem}
select,input[type=number],input[type=text]{background:#222;border:1px solid #444;color:#eee;padding:.4rem;border-radius:4px;width:100%;box-sizing:border-box;font-size:.95rem}
input[type=range]{width:100%}
.chk{display:flex;align-items:center;gap:.5rem}
.chk input{width:auto}
button{background:#2563eb;border:0;color:#fff;padding:.55rem 1.3rem;border-radius:4px;cursor:pointer;font-size:1rem;margin-top:1rem}
button:disabled{background:#333;color:#777;cursor:default}
button.fg{background:#7f1d1d}
button.fg:disabled{background:#333;color:#777}
#status{min-height:1.2em;margin-left:1rem}
.dirty{color:#facc15}
.ok{color:#4ade80}
.err{color:#f87171}
hr{border:0;border-top:1px solid #333;margin:2rem 0}
h2{font-size:1.1rem;margin-bottom:.3rem}
.bond{display:flex;align-items:center;gap:.5rem;flex-wrap:wrap;padding:.5rem 0;border-bottom:1px solid #222}
.bond .nm{flex:1 1 8rem;min-width:0;background:#222;border:1px solid #444;color:#eee;padding:.35rem;border-radius:4px;font-size:.9rem}
.bond .addr{color:#888;font-size:.78rem;font-family:monospace}
.bond .dot{color:#4ade80;font-size:.78rem;white-space:nowrap}
.bond button{margin:0;padding:.35rem .7rem;font-size:.85rem;background:#3a3a3a;flex:none}
.bond button.fg,button.fg{background:#7f1d1d}
.btns{display:flex;gap:.5rem;align-items:center}
#bonds_empty{color:#888;font-size:.9rem}
#statuscard{display:flex;align-items:center;gap:1rem;flex-wrap:wrap;background:#1a1a1a;border:1px solid #333;border-radius:8px;padding:.7rem 1rem;margin:1rem 0}
#statuscard .dot{width:.6rem;height:.6rem;border-radius:50%;background:#555;flex:none}
#statuscard.on .dot{background:#4ade80}
#statuscard .s{font-size:.9rem}
#statuscard .s b{color:#fff}
#statuscard .muted{color:#888}
.batt{display:inline-flex;align-items:center;gap:.35rem}
.batt .bar{width:34px;height:14px;border:1px solid #888;border-radius:2px;position:relative;padding:1px}
.batt .bar::after{content:"";position:absolute;right:-3px;top:4px;width:2px;height:6px;background:#888}
.batt .fill{height:100%;background:#4ade80;border-radius:1px}
.batt.low .fill{background:#f87171}
footer{margin:2.5rem 0 1rem;padding-top:1rem;border-top:1px solid #333;display:flex;gap:1rem;flex-wrap:wrap;align-items:center;font-size:.85rem;color:#888}
footer a{color:#60a5fa;text-decoration:none}
footer a:hover{text-decoration:underline}
footer .kofi{color:#fff;background:#13c3ff;padding:.3rem .7rem;border-radius:4px}
footer .kofi:hover{text-decoration:none;opacity:.9}
nav.tabs{display:flex;gap:.3rem;flex-wrap:wrap;margin:1.2rem 0 .5rem;border-bottom:1px solid #333}
nav.tabs button{background:none;border:0;border-bottom:2px solid transparent;color:#888;padding:.5rem .8rem;margin:0;border-radius:0;font-size:.95rem;cursor:pointer}
nav.tabs button:hover{color:#ccc}
nav.tabs button.active{color:#fff;border-bottom-color:#2563eb}
section.tab{display:none}
section.tab.active{display:block}
section.tab>h2:first-child{margin-top:.3rem}
</style></head><body>
<h1>DS5-Linux-Bridge <small id="ver"></small></h1>
<p>Adapter configuration. Changes are saved to the adapter's flash.</p>

<nav class="tabs">
  <button data-tab="main" class="active">Controller</button>
  <button data-tab="bonds">Paired</button>
  <button data-tab="net" id="tabbtn_net">Network</button>
  <button data-tab="wake" id="tabbtn_wake">Wake / WOL</button>
  <button data-tab="update" id="tabbtn_update">Update</button>
</nav>

<section id="tab-main" class="tab active">
<div id="statuscard">
  <span class="dot"></span>
  <span class="s" id="st_conn">Checking…</span>
  <span class="s batt" id="st_batt" style="display:none">
    <span class="bar"><span class="fill" id="st_fill"></span></span>
    <span id="st_pct"></span>
  </span>
  <div id="st_slots" style="display:none;margin-top:.35rem;font-size:.9rem;color:#aaa"></div>
</div>

<div class="field" id="poweroffall_field" style="display:none">
  <button id="poweroffall" type="button" class="fg">Power off all controllers</button>
  <span id="postatus"></span>
</div>

<div class="field">
  <label class="lbl">Controller mode</label>
  <select id="controller_mode">
    <option value="2">Auto-detect</option>
    <option value="0">DualSense (DS5)</option>
    <option value="1">DualSense Edge (DSE)</option>
  </select>
  <div class="hint">Takes effect after reconnecting the controller.</div>
</div>

<div class="field">
  <label class="lbl">Polling rate</label>
  <select id="polling_rate_mode">
    <option value="0">250 Hz</option>
    <option value="1">500 Hz</option>
    <option value="2">Real-time (1000 Hz)</option>
  </select>
  <div class="hint">Takes effect after reconnecting the controller.</div>
</div>

<div class="field">
  <label class="lbl">Audio buffer length: <span id="ab_val"></span></label>
  <input type="range" id="audio_buffer_length" min="16" max="128" step="1">
  <div class="hint">Lower = less latency, higher = more stutter resistance (16-128).</div>
</div>

<div class="field">
  <label class="lbl">Inactivity timeout: <span id="it_val"></span> min</label>
  <input type="range" id="inactive_time" min="5" max="60" step="1">
  <div class="hint">Disconnect the controller after this idle time.</div>
</div>

<div class="field chk">
  <input type="checkbox" id="disable_inactive_disconnect">
  <label for="disable_inactive_disconnect">Never auto-disconnect on inactivity</label>
</div>

<div class="field chk">
  <input type="checkbox" id="disable_pico_led">
  <label for="disable_pico_led">Disable the onboard Pico LED</label>
</div>

<div class="field chk" id="multi_field" style="display:none">
  <input type="checkbox" id="multi_allowed">
  <label for="multi_allowed">Allow multiple controllers simultaneously (up to <span id="multi_max">4</span>)</label>
</div>
<div class="hint" id="multi_hint" style="display:none">Off by default: the
  adapter connects one controller at a time, exactly as before. When enabled,
  two or more controllers can play together &mdash; the adapter then presents
  one plain gamepad per controller and no audio: it briefly re-plugs itself
  each time a <b>new</b> player joins, but never when someone leaves.
  Controller audio / HD haptics are single-controller features (classic
  rumble works for everyone); to get them back after a multi session, power
  <b>all</b> controllers off, then reconnect one. Turning this back off keeps
  an already-connected group playing; it applies to new connections.</div>

<div>
  <button id="save">Save</button>
  <button id="factoryreset" class="fg">Factory reset</button>
  <span id="status"></span>
</div>
<div class="hint">Factory reset restores all settings above to defaults. Paired
  controllers are kept (use <b>Forget all</b> in the Paired tab to remove those).</div>
</section>

<section id="tab-bonds" class="tab">
<h2>Paired controllers</h2>
<div class="hint">Controllers the adapter remembers. The adapter holds up to
  <span id="bond_max">4</span>. Once a controller is paired the adapter stops
  looking for new ones (a remembered controller reconnects on its own) &mdash;
  use <b>Pair new controller</b> to add another, or forget one to free a slot.</div>
<div id="bonds"></div>
<div id="bonds_empty" style="display:none">No paired controllers stored.</div>
<div class="btns">
  <button id="pair">Pair new controller</button>
  <button id="forgetall" class="fg">Forget all</button>
  <span id="bstatus"></span>
</div>
</section>

<section id="tab-net" class="tab">
<div id="wol_section" style="display:none">
<h2>Network</h2>
<div class="hint">AP setup mode is onboarding-only — the controller and the full
  config work once the adapter is on your home WiFi (STA mode).</div>
<div class="field">
  <label class="lbl">Device name</label>
  <input id="hostname" type="text" inputmode="latin" maxlength="10"
         placeholder="ds5" pattern="[A-Za-z0-9-]{1,10}">
  <div class="hint">The name this adapter uses on your network — reach the page at
    <code>http://&lt;name&gt;.local/</code>. Give each adapter a unique name if you
    run more than one (otherwise they collide on <code>ds5.local</code>).
    Letters, digits and hyphens only. Takes effect after the adapter reboots.</div>
</div>
<div class="btns">
  <button id="net_save">Save</button>
  <span id="nstatus"></span>
</div>
<div class="field" id="diag_field">
  <label class="lbl">Diagnostics</label>
  <div class="field chk" style="margin:.2rem 0">
    <input type="checkbox" id="weblog_enabled">
    <label for="weblog_enabled">Capture firmware log (readable at
      <a href="/api/log" target="_blank" rel="noopener">/api/log</a>)</label>
  </div>
  <div class="hint">Off by default. When on, the adapter mirrors its diagnostic
    output into a small memory buffer you can open in a browser tab and
    copy-paste into a bug report. Survives reboots (enable it, reproduce the
    problem — including unplugging/replugging — then open the log). Kept in
    RAM only; it never writes to flash and is lost on power-off.</div>
  <div class="btns">
    <button id="diag_save">Save</button>
    <span id="dstatus"></span>
  </div>
</div>

<div class="field" id="wifi_reset_field" style="display:none">
  <button id="wifi_reset" type="button" class="fg">Reset saved WiFi</button>
  <span id="wrstatus"></span>
</div>
</div>
</section>

<section id="tab-wake" class="tab">
<div id="wake_section" style="display:none">
<h2>Wake keyboard</h2>
<div class="field chk">
  <input type="checkbox" id="wake_kbd_enabled">
  <label for="wake_kbd_enabled">USB wake keyboard (wake the PC from sleep)</label>
</div>
<div class="hint">Adds a tiny keyboard to the adapter's USB identity that types a
  silent key (F15) so a controller press can wake the PC from sleep (S3) on
  machines where plain USB wake doesn't work. Trade-off: the adapter no longer
  looks like a pure DualSense over USB &mdash; some anticheat software may notice
  the extra keyboard. Wake-on-LAN (below) does not need this. Saving applies
  immediately: the adapter briefly re-plugs itself.</div>
<div class="btns">
  <button id="wake_save">Save</button>
  <span id="kstatus"></span>
</div>
</div>

<div id="wol_section2" style="display:none">
<h2>Wake-on-LAN</h2>
<div class="hint">Wake a PC over the network by sending it a magic packet. Useful
  when the PC is fully off (S4/S5) and USB wake isn't supported by its
  motherboard. Press the controller's PS button to wake, or use the button here.
  The target PC must have Wake-on-LAN enabled in its BIOS and OS network driver.</div>
<div class="field">
  <label class="lbl">Target PC MAC address</label>
  <input id="wol_target_mac" type="text" inputmode="latin"
         placeholder="AA:BB:CC:DD:EE:FF" pattern="([0-9A-Fa-f]{2}[:\-]?){5}[0-9A-Fa-f]{2}">
  <div class="hint">The network adapter (NIC) MAC of the PC to wake. Don't know
    it? Enter the PC's IP address below and click "Find MAC" — the adapter will
    look it up on your network automatically.</div>
  <div style="margin-top:.4rem;display:flex;gap:.5rem;align-items:center">
    <input id="wol_resolve_ip" type="text" inputmode="decimal" style="flex:1"
           placeholder="PC's IP, e.g. 192.168.1.50" pattern="\d{1,3}(\.\d{1,3}){3}">
    <button id="wol_resolve" type="button">Find MAC</button>
  </div>
</div>
<div class="field">
  <label class="lbl">Second target (optional, e.g. a TV)</label>
  <input id="wol_target_mac2" type="text" inputmode="latin"
         placeholder="AA:BB:CC:DD:EE:FF" pattern="([0-9A-Fa-f]{2}[:\-]?){5}[0-9A-Fa-f]{2}">
  <div class="hint">A second device to wake alongside the PC. Leave blank if you
    only wake one device. "Wake now" (and a controller press) wakes both.</div>
</div>
<div class="btns">
  <button id="wol_save">Save</button>
  <button id="wol_wake">Wake now</button>
  <span id="wstatus"></span>
</div>
</div>
</section>

<section id="tab-update" class="tab">
<h2>Firmware update</h2>
<div class="hint">Installs the latest release for this board straight from GitHub.
  The adapter reboots into a small updater, is offline for about a minute, then
  comes back on the new version. Settings, WiFi and paired controllers are kept.
  If anything fails (network, download, checksum) nothing is changed and it
  reboots back unchanged. <b>Do not unplug</b> while the status shows
  &ldquo;installing&rdquo; &mdash; interrupting that step requires reflashing over
  USB (hold BOOTSEL while plugging in, copy the release .uf2).</div>
<div class="field">
  <label class="lbl">Installed version</label>
  <span id="ota_cur">&mdash;</span>
</div>
<div class="field">
  <label class="lbl">Latest available</label>
  <span id="ota_avail">checking&hellip;</span>
</div>
<div class="field chk">
  <input type="checkbox" id="ota_beta">
  <label for="ota_beta">Include pre-releases (beta channel)</label>
</div>
<div class="field chk">
  <input type="checkbox" id="ota_force">
  <label for="ota_force">Reinstall even if already on the latest version</label>
</div>
<div class="btns">
  <button id="ota_go">Install latest</button>
  <span id="otstatus"></span>
</div>
<div class="hint" id="ota_last" style="display:none"></div>
</section>

<script>
const $=id=>document.getElementById(id);
function setStatus(msg,cls){const s=$('status');s.className=cls||'';s.textContent=msg}

// ----- Tabs (single page, client-side show/hide; selection kept in the hash) -----
function showTab(id){
  const btn=document.querySelector('nav.tabs button[data-tab="'+id+'"]');
  if(!btn||btn.style.display==='none'){id='main';}  // fall back if tab is hidden
  document.querySelectorAll('section.tab').forEach(s=>
    s.classList.toggle('active',s.id==='tab-'+id));
  document.querySelectorAll('nav.tabs button').forEach(b=>
    b.classList.toggle('active',b.dataset.tab===id));
}
document.querySelectorAll('nav.tabs button').forEach(b=>
  b.onclick=()=>{location.hash=b.dataset.tab;showTab(b.dataset.tab);});
window.addEventListener('hashchange',()=>showTab(location.hash.slice(1)||'main'));

function bindRange(id,out){const el=$(id);const fn=()=>$(out).textContent=el.value;el.oninput=()=>{fn();markDirty()};return fn}
const upd=[bindRange('audio_buffer_length','ab_val'),bindRange('inactive_time','it_val')];

function markDirty(){$('save').disabled=false;setStatus('unsaved changes','dirty')}
['controller_mode','polling_rate_mode','disable_inactive_disconnect','disable_pico_led']
  .forEach(id=>$(id).onchange=markDirty);

window.multiOn=false;
window.connectedPads=0;
function updatePowerOffAllVisibility(){
  $('poweroffall_field').style.display=window.connectedPads>=2?'':'none';
}
$('multi_allowed').onchange=()=>{
  window.multiOn=$('multi_allowed').checked;
  updatePowerOffAllVisibility();
  markDirty();
};

async function load(){
  try{
    const c=await (await fetch('/api/config')).json();
    $('ver').textContent=c.version;
    $('controller_mode').value=c.controller_mode;
    $('polling_rate_mode').value=c.polling_rate_mode;
    $('audio_buffer_length').value=c.audio_buffer_length;
    $('inactive_time').value=c.inactive_time;
    $('disable_inactive_disconnect').checked=!!c.disable_inactive_disconnect;
    $('disable_pico_led').checked=!!c.disable_pico_led;
    // Network + Wake-on-LAN. wol_target_mac[2] are 12 hex chars, all-zero ==
    // unset. (wol_capable/wifi_capable are always true on current firmware; the
    // gates keep the page working against older firmware -- when a gate is false
    // its tab has no content, so hide the whole tab button too.)
    if(c.wol_capable){
      $('wol_section').style.display='';   // Network tab: device name
      $('wol_section2').style.display='';  // Wake tab: Wake-on-LAN
      // Always reflect the current saved name (server returns a sanitized,
      // never-empty hostname). Assign unconditionally so the box shows the real
      // value, not the placeholder, after a rename.
      $('hostname').value=c.hostname||'';
      if(c.wol_target_mac&&c.wol_target_mac!=='000000000000')
        $('wol_target_mac').value=fmtAddr(c.wol_target_mac);
      if(c.wol_target_mac2&&c.wol_target_mac2!=='000000000000')
        $('wol_target_mac2').value=fmtAddr(c.wol_target_mac2);
    }
    if(c.wifi_capable){
      $('wifi_reset_field').style.display='';
    }
    // Wake section: only firmware with the dynamic-descriptor machinery can
    // enumerate the wake keyboard.
    if(c.wake_kbd_capable){
      $('wake_section').style.display='';
      $('wake_kbd_enabled').checked=!!c.wake_kbd_enabled;
    }
    // Multi-controller toggle: only shown when the firmware was built with
    // more than one slot.
    window.multiOn=false;
    if(c.multi_capable){
      $('multi_field').style.display='';
      $('multi_hint').style.display='';
      $('multi_allowed').checked=!!c.multi_allowed;
      $('multi_max').textContent=c.multi_slots;
      window.multiOn=!!c.multi_allowed;
    }
    updatePowerOffAllVisibility();
    // Diagnostic log toggle (any firmware serving this page supports it).
    $('weblog_enabled').checked=!!c.weblog_enabled;
    // Hide a tab's nav button when the whole tab is empty on this firmware.
    if(!c.wol_capable){$('tabbtn_net').style.display='none';}
    if(!c.wol_capable&&!c.wake_kbd_capable){$('tabbtn_wake').style.display='none';}
    // OTA: Pico W (2MB flash) and custom no-OTA builds can't self-update.
    if(!c.ota_capable){$('tabbtn_update').style.display='none';}
    else{$('ota_cur').textContent=c.version;otaShowLast();otaCheckLatest(c.ota_repo);}
    // Re-apply the selected tab now that hidden buttons are known (a deep-link
    // to a now-hidden tab falls back to Controller).
    showTab(location.hash.slice(1)||'main');
    upd.forEach(f=>f());
    $('save').disabled=true;setStatus('');
  }catch(e){setStatus('load failed','err')}
}

async function save(){
  const fields=[
    'controller_mode='+$('controller_mode').value,
    'polling_rate_mode='+$('polling_rate_mode').value,
    'audio_buffer_length='+$('audio_buffer_length').value,
    'inactive_time='+$('inactive_time').value,
    'disable_inactive_disconnect='+($('disable_inactive_disconnect').checked?1:0),
    'disable_pico_led='+($('disable_pico_led').checked?1:0)
  ];
  // Only firmware that showed the toggle should receive it.
  if($('multi_field').style.display!=='none'){
    fields.push('multi_allowed='+($('multi_allowed').checked?1:0));
    window.multiOn=$('multi_allowed').checked;
  }
  const body=fields.join('&');
  setStatus('saving…','dirty');
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    if(r.ok){$('save').disabled=true;setStatus('Saved ✓','ok')}
    else setStatus('save failed — not written to flash, try again','err');
  }catch(e){setStatus('save failed','err')}
}

$('save').onclick=save;

async function factoryReset(){
  if(!confirm('Reset all settings to defaults? Paired controllers are kept.'))return;
  setStatus('resetting…','dirty');
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'factory_reset=1'});
    if(r.ok){setStatus('Reset ✓ — reloading','ok');setTimeout(()=>location.reload(),600)}
    else setStatus('reset failed — not written to flash, try again','err');
  }catch(e){setStatus('reset failed','err')}
}
$('factoryreset').onclick=factoryReset;

function poStatus(msg,cls){const s=$('postatus');s.className=cls||'';s.textContent=msg}
async function powerOffAll(){
  if(!confirm('Power off all connected controllers? Their pairings will be kept.'))return;
  const b=$('poweroffall');
  b.disabled=true;
  poStatus('sending…','dirty');
  try{
    const r=await fetch('/api/controllers',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'action=poweroffall'});
    if(r.ok){poStatus('Power-off sent ✓','ok');setTimeout(loadStatus,500)}
    else poStatus('power-off failed','err');
  }catch(e){poStatus('power-off failed','err')}
  finally{b.disabled=false}
}
$('poweroffall').onclick=powerOffAll;

// ----- Firmware update (OTA from GitHub Releases) -----
function ostatus(msg,cls){const s=$('otstatus');s.className=cls||'';s.textContent=msg}
const OTA_STATES={wifi:'joining WiFi…',check:'checking latest release…',
  sha:'fetching checksum…',download:'downloading…',verify:'verifying…',
  apply:'installing — DO NOT unplug!'};
let otaTimer=null;

// Shown on page load: outcome of the most recent update attempt, if any
// (persisted across the reboot back; cleared by power-off).
async function otaShowLast(){
  try{
    const d=await (await fetch('/api/ota/status')).json();
    if(d.mode==='idle'&&d.last_result>0){
      const el=$('ota_last');el.style.display='';
      el.textContent='Last update attempt: '+d.last_result_str;
    }
  }catch(e){}
}

// "Update available" check, done by the BROWSER against api.github.com
// (CORS-open; the browser does the TLS — the adapter never does any in
// normal mode). Purely informational: the adapter re-resolves and
// checksum-verifies everything itself at install time. The beta checkbox
// picks between newest-stable and newest-including-prereleases, mirroring
// which release the updater will install (persisted in localStorage only —
// it's a per-browser display preference plus a per-request flag).
window.otaRel={stable:null,beta:null};
function otaAvailText(){
  const beta=$('ota_beta').checked;
  const r=beta?(window.otaRel.beta||window.otaRel.stable):window.otaRel.stable;
  const cur=$('ota_cur').textContent;
  if(!r){$('ota_avail').textContent='unknown (couldn’t reach GitHub from this browser)';return}
  const label=r.tag+(r.pre?' (pre-release)':'');
  if(r.tag===cur){$('ota_avail').textContent=label+' — up to date ✓';}
  else{$('ota_avail').innerHTML='';
    const b=document.createElement('b');b.textContent=label+' — update available';
    $('ota_avail').appendChild(b);}
}
async function otaCheckLatest(repo){
  try{
    const rels=await (await fetch('https://api.github.com/repos/'+repo+'/releases?per_page=15')).json();
    if(Array.isArray(rels)){
      for(const r of rels){
        if(r.draft)continue;
        if(!window.otaRel.beta)window.otaRel.beta={tag:r.tag_name,pre:r.prerelease};
        if(!r.prerelease&&!window.otaRel.stable){window.otaRel.stable={tag:r.tag_name,pre:false};break}
      }
    }
  }catch(e){}
  otaAvailText();
}
$('ota_beta').checked=localStorage.getItem('ota_beta')==='1';
$('ota_beta').onchange=()=>{localStorage.setItem('ota_beta',$('ota_beta').checked?'1':'0');otaAvailText()};

async function otaPoll(){
  try{
    const d=await (await fetch('/api/ota/status',{cache:'no-store'})).json();
    if(d.mode==='updating'){
      let msg=OTA_STATES[d.state]||d.state;
      if(d.state==='download'&&d.total>0)
        msg+=' '+Math.round(100*d.bytes/d.total)+'%';
      if(d.tag)msg+=' ('+d.tag+')';
      ostatus(msg,'dirty');
    }else{
      // Back in normal mode: the attempt finished (either the new firmware
      // booted, or the old one is reporting why it didn't change).
      clearInterval(otaTimer);otaTimer=null;
      $('ota_go').disabled=false;
      $('ota_cur').textContent=d.version;
      const ok=(d.last_result===1||d.last_result===2);
      ostatus(d.last_result_str+' — now on '+d.version,ok?'ok':'err');
    }
  }catch(e){
    // Expected while the adapter reboots (twice) around the update.
    ostatus('adapter offline (rebooting)…','dirty');
  }
}

$('ota_go').onclick=async()=>{
  const ch=$('ota_beta').checked?'latest release INCLUDING pre-releases':'latest stable release';
  if(!confirm('Install the '+ch+' from GitHub?\nThe adapter goes offline for about a minute. Do NOT unplug it while the status shows "installing".'))return;
  const body='force='+($('ota_force').checked?1:0)+'&beta='+($('ota_beta').checked?1:0);
  try{
    const r=await fetch('/api/ota/start',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    if(!r.ok){ostatus('failed to start','err');return;}
    $('ota_go').disabled=true;
    ostatus('rebooting into updater…','dirty');
    // First polls will fail while it reboots; the poll handler shows that.
    otaTimer=setInterval(otaPoll,2000);
  }catch(e){ostatus('failed to start','err')}
};

// ----- Paired controllers -----
function fmtAddr(h){return h.match(/.{2}/g).join(':')}
function bstatus(msg,cls){const s=$('bstatus');s.className=cls||'';s.textContent=msg}

async function loadBonds(){
  try{
    const d=await (await fetch('/api/bonds')).json();
    $('bond_max').textContent=d.max;
    const box=$('bonds');box.innerHTML='';
    const bonds=d.bonds||[];
    $('bonds_empty').style.display=bonds.length?'none':'block';
    // connected_all lists every live pad (multi-slot); fall back to the
    // legacy single-connected field against older firmware.
    const conn=d.connected_all||(d.connected?[d.connected]:[]);
    bonds.forEach(b=>{
      const connected=conn.includes(b.addr);
      const row=document.createElement('div');row.className='bond';
      const nm=document.createElement('input');
      nm.className='nm';nm.maxLength=15;nm.value=b.name;
      nm.placeholder=connected?'(connected)':'unnamed';
      const meta=document.createElement('span');meta.className='addr';
      meta.textContent=fmtAddr(b.addr);
      const dot=document.createElement('span');dot.className='dot';
      dot.textContent=connected?'● connected':'';
      const ren=document.createElement('button');ren.textContent='Rename';
      ren.onclick=()=>renameBond(b.addr,nm.value);
      const fg=document.createElement('button');fg.className='fg';fg.textContent='Forget';
      fg.onclick=()=>forgetBond(b.addr,nm.value||fmtAddr(b.addr));
      row.append(nm,meta,dot,ren,fg);
      box.appendChild(row);
    });
    bstatus('');
  }catch(e){bstatus('load failed','err')}
}

async function postBonds(body,msg){
  bstatus(msg,'dirty');
  try{
    const r=await fetch('/api/bonds',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    if(r.ok){bstatus('Done ✓','ok');loadBonds()}
    else bstatus('failed','err');
  }catch(e){bstatus('failed','err')}
}
function renameBond(addr,name){
  postBonds('action=rename&addr='+addr+'&name='+encodeURIComponent(name),'saving…');
}
function forgetBond(addr,label){
  if(!confirm('Forget "'+label+'"?\nYou will need to re-pair it (Share + PS).'))return;
  postBonds('action=forget&addr='+addr,'forgetting…');
}
$('pair').onclick=()=>{
  // Multi mode keeps connected controllers playing while pairing; single mode
  // (or the toggle off) swaps the active controller out first.
  const msg=window.multiOn
    ?'Pair a new controller?\nConnected controllers keep playing. Put the new controller in pairing mode (hold Share + PS until the light bar flashes).'
    :'Pair a new controller?\nThe controller you are using now will disconnect (it stays remembered and reconnects later). Then put the new controller in pairing mode (hold Share + PS until the light bar flashes).';
  if(!confirm(msg))return;
  postBonds('action=pair','opening pairing…');
};
$('forgetall').onclick=()=>{
  if(!confirm('Forget ALL paired controllers?\nEach will need to be re-paired.'))return;
  postBonds('action=forgetall','forgetting all…');
};

// ----- Wake (USB wake keyboard) -----
function kstatus(msg,cls){const s=$('kstatus');s.className=cls||'';s.textContent=msg}
$('wake_kbd_enabled').onchange=()=>kstatus('unsaved change','dirty');
$('wake_save').onclick=async()=>{
  kstatus('saving…','dirty');
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'wake_kbd_enabled='+($('wake_kbd_enabled').checked?1:0)});
    if(r.ok)kstatus('Saved ✓ — adapter re-plugs briefly','ok');
    else kstatus('save failed — not written to flash, try again','err');
  }catch(e){kstatus('save failed','err')}
};

// ----- Network (device name) -----
function nstatus(msg,cls){const s=$('nstatus');s.className=cls||'';s.textContent=msg}
$('net_save').onclick=async()=>{
  const fields=[
    'hostname='+encodeURIComponent($('hostname').value.trim())
  ];
  nstatus('saving…','dirty');
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:fields.join('&')});
    if(r.ok)nstatus('Saved ✓ (reboot adapter to apply)','ok');
    else nstatus('save failed','err');
  }catch(e){nstatus('save failed','err')}
};

// ----- Diagnostics (firmware log toggle) -----
function dstatus(msg,cls){const s=$('dstatus');s.className=cls||'';s.textContent=msg}
$('weblog_enabled').onchange=()=>dstatus('unsaved change','dirty');
$('diag_save').onclick=async()=>{
  dstatus('saving…','dirty');
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'weblog_enabled='+($('weblog_enabled').checked?1:0)});
    if(r.ok)dstatus('Saved ✓','ok');
    else dstatus('save failed — not written to flash, try again','err');
  }catch(e){dstatus('save failed','err')}
};

function wrstatus(msg,cls){const s=$('wrstatus');s.className=cls||'';s.textContent=msg}
$('wifi_reset').onclick=async()=>{
  if(!confirm('Reset saved WiFi credentials?\nThe adapter will reboot into setup AP mode.'))return;
  wrstatus('resetting...','dirty');
  $('wifi_reset').disabled=true;
  try{
    const r=await fetch('/api/wifi_reset',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'action=reset'});
    const d=await r.json();
    if(r.ok&&d.ok)wrstatus('Rebooting to setup AP...','ok');
    else{wrstatus('reset failed','err');$('wifi_reset').disabled=false}
  }catch(e){
    wrstatus('Rebooting to setup AP...','ok');
  }
};

// ----- Wake-on-LAN -----
function wstatus(msg,cls){const s=$('wstatus');s.className=cls||'';s.textContent=msg}
// Normalize "AA:BB:..", "aa-bb-..", "aabb.." -> 12 upper-hex chars, or '' if invalid.
function macHex(s){const h=s.replace(/[:\-.\s]/g,'').toUpperCase();return /^[0-9A-F]{12}$/.test(h)?h:''}
$('wol_save').onclick=async()=>{
  // Both MAC fields: empty is allowed (clears that target -> all-zero "unset");
  // only a non-empty-but-malformed MAC is an error.
  const raw=$('wol_target_mac').value.trim();
  const raw2=$('wol_target_mac2').value.trim();
  let h='000000000000',h2='000000000000';
  if(raw){h=macHex(raw);if(!h){wstatus('invalid MAC','err');return}}
  if(raw2){h2=macHex(raw2);if(!h2){wstatus('invalid 2nd MAC','err');return}}
  wstatus('saving…','dirty');
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'wol_target_mac='+h+'&wol_target_mac2='+h2});
    if(r.ok){wstatus('Saved ✓','ok');if(raw)$('wol_target_mac').value=fmtAddr(h);if(raw2)$('wol_target_mac2').value=fmtAddr(h2)}
    else wstatus('save failed','err');
  }catch(e){wstatus('save failed','err')}
};
$('wol_wake').onclick=async()=>{
  // Wake ALL stored targets: POST action=wake with no mac so the server fires a
  // magic packet to every configured (non-zero) target.
  wstatus('sending magic packet…','dirty');
  try{
    const r=await fetch('/api/wol',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'action=wake'});
    if(r.ok)wstatus('Magic packet sent ✓','ok');
    else wstatus('send failed','err');
  }catch(e){wstatus('send failed','err')}
};
$('wol_resolve').onclick=async()=>{
  const ip=$('wol_resolve_ip').value.trim();
  if(!/^\d{1,3}(\.\d{1,3}){3}$/.test(ip)){wstatus('enter a valid IP first','err');return}
  wstatus('looking up MAC…','dirty');
  $('wol_resolve').disabled=true;
  try{
    await fetch('/api/resolve_mac',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ip='+encodeURIComponent(ip)});
    // The POST only starts the ARP lookup (the adapter can't block the
    // response on it); poll the GET until it reports a final answer.
    let r=null;
    for(let i=0;i<20;i++){
      r=await (await fetch('/api/resolve_mac')).json();
      if(!r.pending)break;
      await new Promise(res=>setTimeout(res,100));
    }
    if(r&&!r.pending&&r.ok){$('wol_target_mac').value=fmtAddr(r.mac);wstatus('Found MAC '+fmtAddr(r.mac)+' ✓','ok')}
    else wstatus('no reply from that IP (is it online?)','err');
  }catch(e){wstatus('lookup failed','err')}
  finally{$('wol_resolve').disabled=false}
};

// ----- Live status (GET /api/status) -----
async function loadStatus(){
  try{
    const s=await (await fetch('/api/status')).json();
    const card=$('statuscard');
    card.className=s.connected?'on':'';
    const slots=(s.slots||[]).map((p,i)=>({...p,n:i+1})).filter(p=>p.connected);
    window.connectedPads=slots.length;
    updatePowerOffAllVisibility();
    if(slots.length>1){
      // Multi-pad: the ONE status card carries everything -- headline with the
      // count, then a per-player line each (battery inline; the single-pad
      // battery bar is hidden, it can't speak for two pads).
      $('st_conn').innerHTML='<b>'+slots.length+' controllers</b> connected';
      $('st_batt').style.display='none';
      $('st_slots').style.display='';
      $('st_slots').innerHTML=slots.map(p=>
        'P'+p.n+' · '+(p.model==='DSE'?'DualSense Edge':'DualSense')
        +(p.battery_valid?(' · '+p.battery_pct+'%'+(p.charging?' ⚡':'')):'')
      ).join('<br>');
    }else if(s.connected){
      $('st_slots').style.display='none';
      $('st_conn').innerHTML='<b>'+(s.model==='DSE'?'DualSense Edge':'DualSense')+'</b> connected';
      if(s.battery_valid){
        $('st_batt').style.display='';
        $('st_pct').textContent=s.battery_pct+'%'+(s.charging?' ⚡':'');
        const f=$('st_fill');f.style.width=s.battery_pct+'%';
        $('st_batt').className='s batt'+((s.battery_pct<=20&&!s.charging)?' low':'');
      }else{$('st_batt').style.display='none'}
    }else{
      $('st_slots').style.display='none';
      $('st_conn').textContent='No controller connected';
      $('st_batt').style.display='none';
    }
  }catch(e){$('st_conn').textContent='status unavailable'}
}

showTab(location.hash.slice(1)||'main'); // initial render before async load resolves
load();
loadBonds();
loadStatus();
setInterval(loadStatus,4000);
</script>
<footer>
  <a href="https://github.com/kungaa/ds5-linux-bridge" target="_blank" rel="noopener">GitHub</a>
  <a class="kofi" href="https://ko-fi.com/mkungaa" target="_blank" rel="noopener">☕ Support on Ko-fi</a>
</footer>
</body></html>
)rawhtml";

#endif // DS5_BRIDGE_WEB_PAGE_H
