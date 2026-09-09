import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';

test('HTML defines each element referenced by the controller', async () => {
  const html = await readFile(new URL('./index.html', import.meta.url), 'utf8');
  const script = await readFile(new URL('./app.mjs', import.meta.url), 'utf8');
  const ids = [...html.matchAll(/\bid="([^"]+)"/g)].map(match => match[1]);
  assert.equal(new Set(ids).size, ids.length, 'Duplicate HTML IDs');
  const required = [...script.matchAll(/\$\('([^']+)'\)/g)].map(match => match[1]);
  for (const key of ['color', 'gradient_end']) {
    required.push(`${key}-picker`, `${key}-x`, `${key}-y`);
  }
  for (const id of required) assert.ok(ids.includes(id), `Missing HTML element: ${id}`);
});
