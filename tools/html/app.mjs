import {SERVICE, uuid, fields, decode, encode, rgbToXy, xyToHex, plan} from './protocol.mjs';
const $ = id => document.getElementById(id);
let device, server, characteristics, current, capabilities, busy = false, generation = 0;
const supported = window.isSecureContext && !!navigator.bluetooth;
function report(message, error = false) {
  $('status').textContent = message;
  $('status').classList.toggle('error',error);
  $('log').textContent = `${new Date().toLocaleTimeString()}  ${message}\n${$('log').textContent}`.slice(0,12000);
}
function buttons() {
  const connected = !!server?.connected;
  $('connect').disabled = !supported || busy || connected;
  $('disconnect').disabled = !connected || busy;
  $('controls').disabled = !connected || !current || busy;
  $('connection').textContent = connected ? (device.name || 'Lighthouse') : 'Disconnected';
}
function getForm() {
  return Object.fromEntries(Object.entries(fields).map(([key,[,type]]) => [key,
    type === 'xy' ? {x:$(key+'-x').valueAsNumber,y:$(key+'-y').valueAsNumber} :
    key === 'on' ? Number($('on').checked) : key === 'brightness' ? Number($('brightness').dataset.exact ?? $('brightness').valueAsNumber/100) :
    type === 'uint' ? $(key).valueAsNumber : Number($(key).value)]));
}
function render(s) {
  for (const [key,[,type]] of Object.entries(fields)) {
    if (type === 'xy') {
      $(key+'-x').value = s[key].x; $(key+'-y').value = s[key].y;
      $(key+'-picker').value = xyToHex(s[key]);
    } else if (key === 'on') $('on').checked = !!s.on;
    else $(key).value = key === 'brightness' ? s[key]*100 : s[key];
  }
  $('brightness').dataset.exact = s.brightness;
  preview();
}
function preview() {
  const s = getForm();
  $('brightness-label').textContent = `${Number((s.brightness*100).toFixed(1))}%`;
  $('ring').replaceChildren(...Array.from({length:6},(_,i) => {
    const t = s.color_mode === 1 ? (i <= 3 ? i/3 : (6-i)/3) : 0;
    const xy = {x:s.color.x*(1-t)+s.gradient_end.x*t,y:s.color.y*(1-t)+s.gradient_end.y*t};
    const led = document.createElement('span'); led.className = 'led';
    led.style.left = `${50+43*Math.sin(i*Math.PI/3)}%`;
    led.style.top = `${50-43*Math.cos(i*Math.PI/3)}%`;
    led.style.setProperty('--led',xyToHex(xy));
    led.style.opacity = s.on ? .15+.85*s.brightness : .1;
    led.title = `LED ${i}`; return led;
  }));
}
function guard(token) {
  if (token !== generation || !server?.connected) throw new Error('Bluetooth disconnected. Reconnect to read the current settings.');
}
async function readAll(token) {
  const snapshot = {};
  for (const key of Object.keys(fields)) {
    guard(token); const value = await characteristics[key].readValue(); guard(token);
    snapshot[key] = decode(key,value);
  }
  current = snapshot; render(snapshot);
}
async function write(key,value,token) {
  guard(token);
  await characteristics[key].writeValueWithResponse(encode(key,value));
  guard(token);
  report(`Accepted: ${key} = ${JSON.stringify(value)}`);
  // Leave time for the four-entry firmware queue to drain; errors remain visible.
  await new Promise(resolve => setTimeout(resolve,80));
  guard(token);
}
async function run(action) {
  if (busy) return;
  busy = true; buttons();
  try { await action(generation); }
  catch (error) { report(error.message || String(error),true); }
  finally { busy = false; buttons(); }
}
$('connect').addEventListener('click',() => run(async () => {
  report('Choose your Lighthouse in the Bluetooth dialog…');
  device = await navigator.bluetooth.requestDevice({filters:[{services:[SERVICE]}]});
  const selected = device;
  const token = ++generation;
  selected.addEventListener('gattserverdisconnected',() => {
    if (device !== selected) return;
    generation++; current = null; characteristics = null; server = null;
    report('Disconnected. The last accepted settings remain on the device.'); buttons();
  },{once:true});
  try {
    server = await selected.gatt.connect(); guard(token);
    const service = await server.getPrimaryService(SERVICE); guard(token);
    const info = await (await service.getCharacteristic(uuid(1))).readValue(); guard(token);
    if (info.byteLength !== 5 || info.getUint8(0) !== 1 || !(info.getUint8(4)&1)) throw new Error('This device does not support Group A protocol v1.');
    capabilities = Array.from(new Uint8Array(info.buffer,info.byteOffset,info.byteLength));
    characteristics = {};
    for (const [key,[id]] of Object.entries(fields)) {
      characteristics[key] = await service.getCharacteristic(uuid(id)); guard(token);
    }
    for (const [key,index] of [['effect',1],['color_mode',2],['shift_mode',3]]) {
      for (const option of $(key).options) option.disabled = !(capabilities[index] & (1 << Number(option.value)));
    }
    await readAll(token); report('Connected. Current Group A settings loaded.');
  } catch (error) { selected.gatt.disconnect(); current = null; throw error; }
}));
$('disconnect').addEventListener('click',() => device?.gatt.disconnect());
$('refresh').addEventListener('click',() => run(async token => { await readAll(token); report('Read current accepted settings; local edits discarded.'); }));
$('off').addEventListener('click',() => run(async token => {
  await write('on',0,token); await readAll(token); report('Group A off. Current settings loaded; local edits discarded.');
}));
$('form').addEventListener('submit',event => {
  event.preventDefault();
  const desired = getForm();
  run(async token => {
    // Refresh the baseline without overwriting the captured edits.
    await readAll(token);
    let steps;
    try { steps = plan(current,desired,capabilities); }
    catch (error) { render(desired); throw error; }
    try {
      for (const [key,value] of steps) await write(key,value,token);
      await readAll(token);
      report(steps.length ? 'Settings accepted and read back from Group A.' : 'Settings already match the device.');
    } catch (error) {
      let recovered = false;
      if (server?.connected && token === generation) {
        try { await readAll(token); recovered = true; } catch { current = null; }
      }
      throw new Error(`Apply stopped: ${error.message}. Some fields may have changed. ${recovered ? 'Accepted settings have been reloaded; review before applying again.' : 'Reconnect to reload the device state.'}`);
    }
  });
});
for (const key of ['color','gradient_end']) {
  $(key+'-picker').addEventListener('input',() => {
    const xy = rgbToXy($(key+'-picker').value);
    $(key+'-x').value = xy.x; $(key+'-y').value = xy.y; preview();
  });
  for (const axis of ['x','y']) $(key+'-'+axis).addEventListener('input',() => { $(key+'-picker').value = xyToHex(getForm()[key]); });
}
$('brightness').addEventListener('input',() => { delete $('brightness').dataset.exact; });
$('form').addEventListener('input',preview);
render({on:1,effect:0,color:rgbToXy('#ff9900'),brightness:1,period_ms:10000,color_mode:1,gradient_end:rgbToXy('#bb55ff'),shift_mode:0,shift_period_ms:7000});
if (!supported) report(!window.isSecureContext ? 'Open this page over localhost or HTTPS to enable Web Bluetooth.' : 'Web Bluetooth is unavailable. Open this page in a supported browser, such as desktop Chrome or Edge.',true);
buttons();
