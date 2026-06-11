import { ImprovSerial } from 'https://unpkg.com/improv-wifi-serial-sdk@2.5.0/dist/serial.js';

const IMPROV_FIRMWARE_NAME = 'Bodenfeuchte Soil Sensor';
const IMPROV_INIT_TIMEOUT_MS = 30000;
const POST_OPEN_RESET_WAIT_MS = 2500;
const PROVISION_TIMEOUT_MS = 45000;

function $(id) {
  return document.getElementById(id);
}

function setStatus(text, kind = '') {
  const el = $('wifi-status');
  el.textContent = text;
  el.className = 'msg' + (kind ? ` ${kind}` : '');
  el.hidden = !text;
}

function setBusy(busy) {
  $('wifi-form').hidden = busy;
  $('wifi-busy').hidden = !busy;
  $('wifi-submit').disabled = busy;
  $('wifi-scan').disabled = busy;
}

function fillNetworks(ssids) {
  const select = $('wifi-ssid-select');
  select.innerHTML = '';
  if (!ssids || ssids.length === 0) {
    const opt = document.createElement('option');
    opt.value = '';
    opt.textContent = 'Kein Netzwerk gefunden — SSID manuell eingeben';
    select.appendChild(opt);
    return;
  }
  for (const net of ssids) {
    const opt = document.createElement('option');
    opt.value = net.name;
    opt.textContent = `${net.name} (${net.rssi} dBm${net.secured ? ', gesichert' : ''})`;
    select.appendChild(opt);
  }
  $('wifi-ssid').value = ssids[0].name;
}

async function openPort() {
  if (!('serial' in navigator)) {
    throw new Error('Web Serial wird in diesem Browser nicht unterstützt (Chrome/Edge nötig).');
  }
  const port = await navigator.serial.requestPort();
  await port.open({ baudRate: 115200, bufferSize: 8192 });
  return port;
}

async function scanNetworks(client) {
  setBusy(true);
  setStatus('Scanne WLAN-Netzwerke …');
  try {
    const ssids = await client.scan();
    fillNetworks(ssids);
    setStatus(`${ssids.length} Netzwerk(e) gefunden.`, 'ok');
  } catch (err) {
    console.error(err);
    setStatus('WLAN-Scan fehlgeschlagen — SSID manuell eingeben.', 'warn');
    fillNetworks([]);
  } finally {
    setBusy(false);
  }
}

export async function startWifiSetup() {
  const dialog = $('wifi-dialog');
  dialog.hidden = false;
  setStatus('USB-Port wird geöffnet …');
  setBusy(true);
  $('wifi-result').hidden = true;

  let port;
  let client;

  try {
    port = await openPort();
    setStatus('USB geöffnet — warte auf Geräte-Neustart …');
    await new Promise((resolve) => setTimeout(resolve, POST_OPEN_RESET_WAIT_MS));
    setStatus('Warte auf Improv (bis 30 s) …');
    client = new ImprovSerial(port, console);
    await client.initialize(IMPROV_INIT_TIMEOUT_MS);

    const info = client.info;
    if (!info || info.firmware !== IMPROV_FIRMWARE_NAME) {
      throw new Error(
        `Unerwartete Firmware: "${info?.firmware || 'unbekannt'}". Bitte zuerst die aktuelle Firmware flashen.`,
      );
    }

    setStatus(`Erkannt: ${info.firmware} ${info.version} (${info.chipFamily})`, 'ok');
    setBusy(false);
    await scanNetworks(client);

    $('wifi-submit').onclick = async () => {
      const ssid =
        $('wifi-ssid').value.trim() ||
        $('wifi-ssid-select').value.trim();
      const password = $('wifi-password').value;
      if (!ssid) {
        setStatus('Bitte eine SSID eingeben.', 'warn');
        return;
      }
      setBusy(true);
      setStatus(`Verbinde mit „${ssid}" …`);
      try {
        await client.provision(ssid, password, PROVISION_TIMEOUT_MS);
        const url = client.nextUrl || 'http://<IP>/';
        $('wifi-result').hidden = false;
        $('wifi-result-link').href = url.startsWith('http') ? url : `http://${url}`;
        $('wifi-result-link').textContent = url;
        setStatus('WLAN gespeichert — Gerät startet neu.', 'ok');
      } catch (err) {
        console.error(err);
        setStatus(`Verbindung fehlgeschlagen: ${err.message || err}`, 'warn');
        setBusy(false);
      }
    };

    $('wifi-scan').onclick = () => scanNetworks(client);
  } catch (err) {
    console.error(err);
    const msg = err.message || String(err);
    if (/not detected/i.test(msg)) {
      setStatus(
        'Improv nicht erkannt — bitte mit „Flash löschen“ neu flashen, USB-Kabel prüfen (Daten, nicht nur Laden), dann erneut „WLAN einrichten“ und bis zu 30 s warten.',
        'warn',
      );
    } else {
      setStatus(msg, 'warn');
    }
    setBusy(false);
    if (client) {
      try {
        await client.close();
      } catch (_) {
        /* ignore */
      }
    }
    if (port) {
      try {
        await port.close();
      } catch (_) {
        /* ignore */
      }
    }
  }
}

export function initWifiSetupUi() {
  $('wifi-open')?.addEventListener('click', () => {
    startWifiSetup().catch((err) => {
      console.error(err);
      setStatus(err.message || String(err), 'warn');
    });
  });
  $('wifi-close')?.addEventListener('click', () => {
    $('wifi-dialog').hidden = true;
    setStatus('');
  });
  $('wifi-ssid-select')?.addEventListener('change', (ev) => {
    const value = ev.target.value;
    if (value) {
      $('wifi-ssid').value = value;
    }
  });
}
