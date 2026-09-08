import http from 'node:http';
import {readFile} from 'node:fs/promises';
// Serve only the client assets, bound to localhost. No dependencies required.
const assets = {'/':'index.html','/index.html':'index.html','/style.css':'style.css','/app.mjs':'app.mjs','/protocol.mjs':'protocol.mjs'};
const mime = {html:'text/html; charset=utf-8',css:'text/css; charset=utf-8',mjs:'text/javascript; charset=utf-8'};
http.createServer(async (req,res) => {
  if (!['GET','HEAD'].includes(req.method)) { res.writeHead(405,{Allow:'GET, HEAD'}).end(); return; }
  const file = assets[new URL(req.url,'http://localhost').pathname];
  if (!file || typeof file !== 'string') { res.writeHead(404).end('Not found'); return; }
  try {
    const content = await readFile(new URL(file,import.meta.url));
    res.writeHead(200,{'Content-Type':mime[file.split('.').pop()],'Cache-Control':'no-store'});
    res.end(req.method === 'HEAD' ? undefined : content);
  } catch { res.writeHead(500).end('Unable to load client asset'); }
}).on('error',error => { console.error(error.message); process.exitCode=1; })
  .listen(8080,'127.0.0.1',() => console.log('Lighthouse controller: http://127.0.0.1:8080\nPress Ctrl+C to stop.'));
