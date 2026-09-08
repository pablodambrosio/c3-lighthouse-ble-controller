export const uuid = id => `8e7f${id.toString(16).padStart(4, '0')}-8f58-4b5c-9d76-2f5a37c41000`;
export const SERVICE = uuid(0);
export const fields = {
  on: [2, 'byte'], effect: [3, 'byte'], color: [4, 'xy'], brightness: [5, 'float'],
  period_ms: [6, 'uint'], color_mode: [7, 'byte'], gradient_end: [8, 'xy'],
  shift_mode: [9, 'byte'], shift_period_ms: [10, 'uint'],
};
export function encode(key, value) {
  const type = fields[key][1];
  const view = new DataView(new ArrayBuffer(type === 'xy' ? 8 : type === 'byte' ? 1 : 4));
  if (type === 'xy') { view.setFloat32(0, value.x, true); view.setFloat32(4, value.y, true); }
  else if (type === 'float') view.setFloat32(0, value, true);
  else if (type === 'uint') view.setUint32(0, value, true);
  else view.setUint8(0, value);
  return view;
}
export function decode(key, view) {
  const type = fields[key][1];
  const size = type === 'xy' ? 8 : type === 'byte' ? 1 : 4;
  if (view.byteLength !== size) throw new Error(`Unexpected ${key} payload length: ${view.byteLength}`);
  if (type === 'xy') return { x: view.getFloat32(0, true), y: view.getFloat32(4, true) };
  return type === 'float' ? view.getFloat32(0, true) : type === 'uint' ? view.getUint32(0, true) : view.getUint8(0);
}
export function rgbToXy(hex) {
  const rgb = hex.match(/[a-f\d]{2}/gi).map(c => parseInt(c, 16) / 255)
    .map(v => v <= .04045 ? v / 12.92 : ((v + .055) / 1.055) ** 2.4);
  const xyz = [[.4124564,.3575761,.1804375],[.2126729,.7151522,.0721750],[.0193339,.1191920,.9503041]]
    .map(row => row.reduce((sum, v, i) => sum + v * rgb[i], 0));
  const total = xyz.reduce((a,b) => a+b, 0);
  return total ? {x: Math.fround(xyz[0]/total), y: Math.fround(xyz[1]/total)} : {x:.3127,y:.329};
}
function linear({x,y}) {
  return [[3.2404542,-1.5371385,-.4985314],[-.969266,1.8760108,.041556],[.0556434,-.2040259,1.0572252]]
    .map(row => row[0]*x/y + row[1] + row[2]*(1-x-y)/y);
}
export function validXy(xy) {
  return Number.isFinite(xy.x) && Number.isFinite(xy.y) && xy.x >= 0 && xy.y > 0 && xy.x + xy.y <= 1 && linear(xy).every(v => v >= -.00001);
}
export function xyToHex(xy) {
  if (!validXy(xy)) return '#ffffff';
  const rgb = linear(xy), peak = Math.max(...rgb);
  return '#' + rgb.map(v => {
    v = Math.max(0,v)/peak;
    return Math.round(255*(v <= .0031308 ? 12.92*v : 1.055*v**(1/2.4)-.055)).toString(16).padStart(2,'0');
  }).join('');
}
export function validate(s, capabilities) {
  if (![0,1].includes(s.on)) throw new Error('Power must be on or off.');
  for (const [key, index, allowed] of [['effect',1,[0,1,2,4]],['color_mode',2,[0,1]],['shift_mode',3,[0,1,2]]]) {
    if (!allowed.includes(s[key]) || !(capabilities[index] & (1 << s[key]))) throw new Error(`Unsupported ${key}.`);
  }
  if (!validXy(s.color)) throw new Error('Start color must be inside the sRGB triangle.');
  if (s.color_mode === 1 && !validXy(s.gradient_end)) throw new Error('Gradient end must be inside the sRGB triangle.');
  if (!Number.isFinite(s.brightness) || s.brightness < 0 || s.brightness > 1) throw new Error('Brightness must be 0–100%.');
  for (const key of ['period_ms','shift_period_ms']) {
    if (!Number.isInteger(s[key]) || s[key] < 0 || s[key] > 4294967295) throw new Error(`${key} must be a whole number from 0 to 4294967295.`);
  }
  if (s.effect === 1 && s.period_ms < 60) throw new Error('Lighthouse rotation must be at least 60 ms.');
  if (s.shift_mode !== 0 && s.shift_period_ms === 0) throw new Error('Moving colors need a nonzero shift period.');
}
// Compare wire values so untouched float32 coordinates are never rewritten through a color picker.
export function changed(key, a, b) {
  return new Uint8Array(encode(key,a).buffer).some((v,i) => v !== new Uint8Array(encode(key,b).buffer)[i]);
}
export function plan(current, desired, capabilities) {
  validate(desired, capabilities);
  const edits = Object.keys(fields).filter(key => key !== 'on' && changed(key,current[key],desired[key]));
  if (edits.includes('gradient_end') && !validXy(desired.gradient_end)) throw new Error('Gradient end must be inside the sRGB triangle.');
  const steps = [];
  const state = structuredClone(current);
  const put = (key,value) => { if (changed(key,state[key],value)) { steps.push([key,value]); state[key]=value; } };
  if (edits.length && state.on) put('on',0);
  // Disable dependent modes before allowing their periods to become zero.
  if (desired.effect !== 1) put('effect',desired.effect);
  if (desired.shift_mode === 0) put('shift_mode',0);
  if (desired.color_mode === 0) put('color_mode',0);
  for (const key of ['color','gradient_end','brightness','period_ms','shift_period_ms','effect','color_mode','shift_mode']) put(key,desired[key]);
  put('on',desired.on);
  return steps;
}
