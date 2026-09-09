import test from 'node:test';
import assert from 'node:assert/strict';
import {uuid,encode,decode,rgbToXy,validXy,plan,validate} from './protocol.mjs';
const caps=[1,0x37,3,7,1];
const base={on:1,effect:0,color:rgbToXy('#ff9900'),brightness:1,period_ms:0,color_mode:0,gradient_end:{x:0,y:0},shift_mode:0,shift_period_ms:0};
test('UUID and wire examples match firmware protocol',() => {
  assert.equal(uuid(10),'8e7f000a-8f58-4b5c-9d76-2f5a37c41000');
  for (const [key,value,hex] of [['brightness',.5,'0000003f'],['period_ms',15000,'983a0000'],['effect',4,'04']]) {
    const wire=encode(key,value); assert.equal(Buffer.from(wire.buffer).toString('hex'),hex); assert.equal(decode(key,wire),value);
  }
  const xy=decode('color',encode('color',{x:.15,y:.06}));
  assert.equal(xy.x,Math.fround(.15)); assert.equal(xy.y,Math.fround(.06));
  assert.throws(() => decode('color',new DataView(new ArrayBuffer(0))));
});
test('picker primaries and black produce valid independent chromaticities',() => {
  for (const color of ['#ff0000','#00ff00','#0000ff','#ffffff','#000000','#ab1234']) assert.ok(validXy(rgbToXy(color)),color);
  assert.equal(validXy({x:.9,y:.05}),false);
});
test('dependency activation and deactivation keep every intermediate command valid',() => {
  const enabled={...base,effect:1,period_ms:60,color_mode:1,gradient_end:rgbToXy('#0000ff'),shift_mode:2,shift_period_ms:7000};
  for (const [from,to] of [[base,enabled],[enabled,{...base,gradient_end:enabled.gradient_end}]]) {
    const steps=plan(from,to,caps), state=structuredClone(from);
    assert.deepEqual(steps[0],['on',0]); assert.deepEqual(steps.at(-1),['on',1]);
    for (const [key,value] of steps) {state[key]=value;validate(state,caps);}
    assert.deepEqual(state,to);
  }
});
test('unchanged inactive endpoint is preserved and no-op sends nothing',() => {
  assert.deepEqual(plan(base,base,caps),[]);
  assert.deepEqual(plan(base,{...base,on:0},caps),[['on',0]]);
  assert.throws(() => plan(base,{...base,gradient_end:{x:.9,y:.05}},caps));
});
test('invalid periods, effects, brightness and capabilities are rejected before writes',() => {
  for (const patch of [{effect:3},{effect:1},{shift_mode:1},{brightness:NaN},{period_ms:2**32},{shift_period_ms:1.5},{color_mode:1}]) assert.throws(() => plan(base,{...base,...patch},caps));
  assert.throws(() => plan(base,{...base,effect:4},[1,1,3,7,1]));
});
test('breathing requires its period before activation and allows zero after deactivation',() => {
  assert.throws(() => plan(base,{...base,effect:5},caps));
  const breathing={...base,effect:5,period_ms:4000};
  for (const [from,to] of [[base,breathing],[breathing,base]]) {
    const state=structuredClone(from);
    for (const [key,value] of plan(from,to,caps)) {state[key]=value;validate(state,caps);}
    assert.deepEqual(state,to);
  }
});

test('Group B capabilities accept its effects and reject Lighthouse',() => {
  const houseCaps=[1,0x35,3,7,3];
  for(const effect of [0,2,4,5]) plan(base,{...base,effect,period_ms:4000},houseCaps);
  assert.throws(()=>plan(base,{...base,effect:1,period_ms:4000},houseCaps));
});
