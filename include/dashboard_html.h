#pragma once

#include <pgmspace.h>

static const char kDashboardHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>Bodenfeuchte</title>
<style>
:root{color-scheme:light dark;--bg:#f6f4f0;--surface:#fff;--border:#ddd8cf;--text:#2c2a26;--muted:#6f6a62;--accent:#4a7352;--accent-text:#fff;--danger:#b85c48;--warn-bg:#f5efe3;--warn-text:#7a5a20;--ok-bg:#e8f0e8;--ok-text:#2f5236;--radius:10px;--font:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
@media (prefers-color-scheme:dark){:root{--bg:#1c1b19;--surface:#2a2825;--border:#3d3a35;--text:#ece8e1;--muted:#a39e95;--accent:#6b9b74;--danger:#d17a66;--warn-bg:#3a3428;--warn-text:#e0c88a;--ok-bg:#243028;--ok-text:#a8d4ae}}
*{box-sizing:border-box}body{margin:0;font-family:var(--font);background:var(--bg);color:var(--text);line-height:1.45}
header{padding:1.1rem 1.25rem;border-bottom:1px solid var(--border);background:var(--surface)}
header h1{margin:0;font-size:1.35rem;font-weight:650;letter-spacing:-.02em}
header .sub{margin:.2rem 0 0;color:var(--muted);font-size:.875rem}
#headerStatus{margin:.45rem 0 0;color:var(--muted);font-size:.8rem}
main{max-width:720px;margin:0 auto;padding:1rem 1rem 0}
.tabs{display:flex;gap:.45rem;margin-bottom:1rem;flex-wrap:wrap}
.tab{padding:.48rem .95rem;border:1px solid var(--border);background:var(--surface);color:var(--text);border-radius:999px;cursor:pointer;font-size:.875rem}
.tab.active{background:var(--accent);border-color:var(--accent);color:var(--accent-text)}
.panel{display:none}.panel.active{display:block}
.card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius);padding:1rem;margin-bottom:1rem}
.card h2{margin:0 0 .75rem;font-size:1rem;font-weight:600}
.moisture-big{font-size:3.2rem;font-weight:700;line-height:1;margin:.25rem 0 .5rem;letter-spacing:-.03em}
.moisture-bar{height:1.1rem;border-radius:999px;background:var(--border);overflow:hidden;margin:.75rem 0}
.moisture-fill{height:100%;border-radius:999px;transition:width .4s ease,background .4s ease}
.meta-grid{display:grid;grid-template-columns:1fr 1fr;gap:.65rem;font-size:.875rem}
.meta-item{padding:.55rem .65rem;background:var(--bg);border-radius:8px}
.meta-item .lbl{display:block;color:var(--muted);font-size:.75rem;margin-bottom:.15rem}
.btn{padding:.45rem .85rem;border:1px solid transparent;border-radius:8px;cursor:pointer;font-weight:600;font-size:.85rem}
.btn-start{background:var(--accent);color:var(--accent-text)}.btn-secondary{background:var(--surface);border-color:var(--border);color:var(--text)}
.btn-small{padding:.3rem .6rem;font-size:.78rem}
input{width:100%;padding:.5rem;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--text);margin-top:.25rem;font:inherit}
label{display:block;margin-top:.65rem;font-size:.84rem;color:var(--muted)}
.row{display:grid;grid-template-columns:1fr 1fr;gap:.75rem}
.msg{padding:.75rem;border-radius:8px;margin-bottom:1rem;font-size:.875rem}
.msg.ok{background:var(--ok-bg);color:var(--ok-text)}.msg.warn{background:var(--warn-bg);color:var(--warn-text)}
.hint{font-size:.84rem;color:var(--muted);margin:0 0 .75rem}
.preview{font-size:1.1rem;font-weight:600;margin-top:.75rem}
.page-footer{text-align:center;padding:1.2rem 1rem 2rem;color:var(--muted);font-size:.8rem}
.page-footer .tagline{display:block;margin-top:.15rem;font-size:.75rem}
@media(max-width:560px){.row,.meta-grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<header>
<h1 id="deviceTitle">Bodenfeuchte</h1>
<p class="sub">ESP8266 D1 Mini · Capacitive Soil Sensor v1.2</p>
<p id="headerStatus">Lade …</p>
</header>
<main>
<div class="tabs">
<button class="tab active" data-tab="status">Status</button>
<button class="tab" data-tab="calibration">Kalibrierung</button>
<button class="tab" data-tab="settings">Einstellungen</button>
<button class="tab" data-tab="firmware">Firmware</button>
</div>
<div id="apHint" class="msg warn" style="display:none">Einrichtungsmodus: Mit <strong id="apName"></strong> verbinden und Dashboard öffnen.</div>
<section id="status" class="panel active">
<div class="card">
<h2 id="sensorLabel">Bodenfeuchte</h2>
<div class="moisture-big" id="moisturePct">— %</div>
<div class="moisture-bar"><div class="moisture-fill" id="moistureBar" style="width:0%"></div></div>
<div class="meta-grid">
<div class="meta-item"><span class="lbl">Roh-ADC</span><span id="rawAdc">—</span></div>
<div class="meta-item"><span class="lbl">Letzte Messung</span><span id="lastMeasure">—</span></div>
<div class="meta-item"><span class="lbl">Kalibrierung trocken</span><span id="calDry">—</span></div>
<div class="meta-item"><span class="lbl">Kalibrierung nass</span><span id="calWet">—</span></div>
</div>
<div style="margin-top:1rem"><button type="button" class="btn btn-start" id="measureBtn">Jetzt messen</button></div>
</div>
</section>
<section id="calibration" class="panel">
<div class="card">
<h2>Kalibrierung</h2>
<p class="hint">Trocken: Sonde in der Luft. Nass: nur die Sonde in Wasser (nicht die Platine). Hoher ADC = trocken, niedriger ADC = nass.</p>
<div class="meta-item" style="margin-bottom:.75rem"><span class="lbl">Aktueller Rohwert</span><span id="calRaw">—</span></div>
<div style="display:flex;gap:.5rem;flex-wrap:wrap;margin-bottom:.75rem">
<button type="button" class="btn btn-secondary" id="calDryBtn">Aktuellen Wert als TROCKEN speichern</button>
<button type="button" class="btn btn-secondary" id="calWetBtn">Aktuellen Wert als NASS speichern</button>
</div>
<form id="calForm">
<div class="row">
<label>dry_adc (trocken)<input id="dryAdc" type="number" min="0" max="1023" required></label>
<label>wet_adc (nass)<input id="wetAdc" type="number" min="0" max="1023" required></label>
</div>
<label>Sensor-Label<input id="sensorLabelInput" maxlength="48"></label>
<div style="margin-top:1rem"><button type="submit" class="btn btn-start">Kalibrierung speichern</button></div>
</form>
<p class="preview">Vorschau: <span id="calPreview">— %</span></p>
<div id="calMsg"></div>
</div>
</section>
<section id="settings" class="panel">
<div id="saveMsg"></div>
<div class="card"><h2>Netzwerk</h2>
<form id="cfgForm">
<label>WLAN<input id="wifiSsid" required></label>
<label>WLAN-Passwort<input id="wifiPass" type="password"></label>
<div class="row"><label>MQTT Host<input id="mqttHost"></label><label>MQTT Port<input id="mqttPort" type="number"></label></div>
<div class="row"><label>MQTT User<input id="mqttUser"></label><label>MQTT Passwort<input id="mqttPass" type="password"></label></div>
<div style="margin-top:1rem;display:flex;gap:.5rem;flex-wrap:wrap">
<button type="submit" class="btn btn-start">Speichern &amp; Neustart</button>
<button type="button" class="btn btn-secondary" id="scanBtn">WLAN scannen</button>
<button type="button" class="btn btn-secondary" id="restartBtn">Neustart</button>
</div>
<select id="scanResults" style="display:none;margin-top:.75rem"></select>
</form></div>
</section>
<section id="firmware" class="panel">
<div class="card"><h2>Firmware</h2>
<p class="hint"><code>pio run -e d1_mini_ota -t upload</code> oder <code>.pio/build/d1_mini/firmware.bin</code> per Web hochladen.</p>
<label>Datei<input id="fwFile" type="file" accept=".bin"></label>
<div style="margin-top:1rem"><button type="button" class="btn btn-start" id="fwUpload">Hochladen</button></div>
<div id="fwMsg" style="margin-top:.75rem"></div>
</div>
</section>
</main>
<footer class="page-footer"><div id="fwVersion">Bodenfeuchte</div></footer>
<script>
const $=s=>document.querySelector(s);
let statusCache={};
document.querySelectorAll('.tab').forEach(t=>t.onclick=()=>{document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));t.classList.add('active');$('#'+t.dataset.tab).classList.add('active')});
async function api(p,o){const r=await fetch(p,o);if(!r.ok){let m=await r.text();try{const j=JSON.parse(m);if(j.error)m=j.error}catch(e){}throw new Error(m)}return r.headers.get('content-type')?.includes('json')?r.json():null}
function fmtSec(s){return s<60?s+' s':Math.floor(s/60)+' min'+(s%60?' '+s%60+' s':'')}
function moistureColor(p){if(p>=60)return'linear-gradient(90deg,#4a9,#6b9)';if(p>=30)return'linear-gradient(90deg,#c9a227,#d4b84a)';return'linear-gradient(90deg,#c45,#d66)'}
function calcPct(raw,dry,wet){if(dry<=wet)return 0;return Math.max(0,Math.min(100,(dry-raw)/(dry-wet)*100))}
function updateCalPreview(){const raw=statusCache.moisture_raw||0;const dry=+$('#dryAdc').value;const wet=+$('#wetAdc').value;$('#calPreview').textContent=calcPct(raw,dry,wet).toFixed(1)+' %'}
function applyStatus(st){statusCache=st;$('#deviceTitle').textContent=st.device_name||'Bodenfeuchte';$('#sensorLabel').textContent=st.sensor_label||'Bodenfeuchte';$('#moisturePct').textContent=(st.moisture_percent!=null?st.moisture_percent.toFixed(1):'—')+' %';const p=st.moisture_percent||0;$('#moistureBar').style.width=p+'%';$('#moistureBar').style.background=moistureColor(p);$('#rawAdc').textContent=st.moisture_raw??'—';$('#lastMeasure').textContent=st.last_measure_sec_ago!=null?'vor '+st.last_measure_sec_ago+' s':'—';$('#calDry').textContent=st.cal_dry_adc??'—';$('#calWet').textContent=st.cal_wet_adc??'—';$('#calRaw').textContent=st.moisture_raw??'—';$('#headerStatus').textContent=(st.ap_mode?'AP · ':'')+(st.wifi_connected?'WLAN '+st.ip:'offline')+(st.mqtt_connected?' · MQTT':'')+' · '+fmtSec(st.uptime_sec);$('#fwVersion').innerHTML=(st.device_name||'Bodenfeuchte')+' · Stand '+(st.firmware_version||'—')+'<span class="tagline">ESP8266 Bodenfeuchte-Sensor</span>';$('#apHint').style.display=st.ap_mode?'block':'none';$('#apName').textContent=st.ap_ssid||'';if(!document.activeElement||document.activeElement.id!=='dryAdc')$('#dryAdc').value=st.cal_dry_adc??'';if(!document.activeElement||document.activeElement.id!=='wetAdc')$('#wetAdc').value=st.cal_wet_adc??'';if(!document.activeElement||document.activeElement.id!=='sensorLabelInput')$('#sensorLabelInput').value=st.sensor_label||'';updateCalPreview()}
async function refresh(){try{const st=await api('/api/status');applyStatus(st)}catch(e){$('#headerStatus').textContent='Verbindung fehlgeschlagen'}}
$('#measureBtn').onclick=async()=>{try{await api('/api/soil/measure',{method:'POST'});setTimeout(refresh,400)}catch(e){alert(e.message)}};
$('#calDryBtn').onclick=async()=>{try{await api('/api/soil/calibrate/dry',{method:'POST'});$('#calMsg').innerHTML='<p class="msg ok">Trockenwert gespeichert.</p>';await refresh()}catch(e){$('#calMsg').innerHTML='<p class="msg warn">'+e.message+'</p>'}};
$('#calWetBtn').onclick=async()=>{try{await api('/api/soil/calibrate/wet',{method:'POST'});$('#calMsg').innerHTML='<p class="msg ok">Nasswert gespeichert.</p>';await refresh()}catch(e){$('#calMsg').innerHTML='<p class="msg warn">'+e.message+'</p>'}};
$('#calForm').onsubmit=async e=>{e.preventDefault();try{await api('/api/soil/calibration',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({dry_adc:+$('#dryAdc').value,wet_adc:+$('#wetAdc').value,label:$('#sensorLabelInput').value})});$('#calMsg').innerHTML='<p class="msg ok">Kalibrierung gespeichert.</p>';await refresh()}catch(e){$('#calMsg').innerHTML='<p class="msg warn">'+e.message+'</p>'}};
['dryAdc','wetAdc'].forEach(id=>$('#'+id).oninput=updateCalPreview);
async function loadConfig(){const c=await api('/api/config');$('#wifiSsid').value=c.wifi_ssid||'';$('#mqttHost').value=c.mqtt_host||'';$('#mqttPort').value=c.mqtt_port||1883;$('#mqttUser').value=c.mqtt_user||''}
$('#cfgForm').onsubmit=async e=>{e.preventDefault();await api('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({wifi_ssid:$('#wifiSsid').value,wifi_password:$('#wifiPass').value,mqtt_host:$('#mqttHost').value,mqtt_port:+$('#mqttPort').value,mqtt_user:$('#mqttUser').value,mqtt_password:$('#mqttPass').value})});$('#saveMsg').innerHTML='<p class="msg ok">Gespeichert — Neustart …</p>'};
$('#scanBtn').onclick=async()=>{const nets=await api('/api/wifi/scan');const s=$('#scanResults');s.style.display='block';s.innerHTML=nets.map(n=>'<option>'+n.ssid+' ('+n.rssi+')</option>').join('');s.onchange=()=>$('#wifiSsid').value=s.value.split(' (')[0]};
$('#restartBtn').onclick=()=>api('/api/restart',{method:'POST'});
$('#fwUpload').onclick=async()=>{const f=$('#fwFile').files[0];if(!f){$('#fwMsg').innerHTML='<p class="msg warn">Bitte .bin wählen.</p>';return}const fd=new FormData();fd.append('firmware',f,f.name);$('#fwMsg').innerHTML='<p class="msg ok">Upload …</p>';try{const r=await fetch('/api/firmware',{method:'POST',body:fd});if(!r.ok)throw new Error(await r.text());$('#fwMsg').innerHTML='<p class="msg ok">Erfolgreich — Neustart …</p>'}catch(e){$('#fwMsg').innerHTML='<p class="msg warn">Fehler: '+e.message+'</p>'}};
loadConfig();refresh();setInterval(refresh,5000);
</script>
</body>
</html>
)rawliteral";
