#!/usr/bin/env node
// webctl: stateless CLI that drives the long-lived webctld daemon over localhost HTTP.
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const PORT = Number(process.env.WEBCTL_PORT || 8765);
const BASE = `http://127.0.0.1:${PORT}`;

const argv = process.argv.slice(2);
const cmd = argv[0];
const rest = argv.slice(1);

// crude flag parser: --key val  and  --flag (boolean)
function flags(args) {
  const f = {};
  const pos = [];
  for (let i = 0; i < args.length; i++) {
    if (args[i].startsWith('--')) {
      const k = args[i].slice(2);
      if (i + 1 < args.length && !args[i + 1].startsWith('--')) f[k] = args[++i];
      else f[k] = true;
    } else pos.push(args[i]);
  }
  return { f, pos };
}

async function api(p, { method = 'GET', body } = {}) {
  const res = await fetch(BASE + p, {
    method,
    headers: body ? { 'Content-Type': 'application/json' } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  });
  const txt = await res.text();
  let j;
  try {
    j = JSON.parse(txt);
  } catch {
    j = { raw: txt };
  }
  if (!res.ok) throw new Error(`${res.status} ${JSON.stringify(j)}`);
  return j;
}

async function isUp() {
  try {
    await api('/health');
    return true;
  } catch {
    return false;
  }
}

async function startDaemon(headless) {
  if (await isUp()) return 'already running';
  const out = fs.openSync(path.join(__dirname, 'daemon.log'), 'a');
  const child = spawn('node', [path.join(__dirname, 'webctld.mjs')], {
    detached: true,
    stdio: ['ignore', out, out],
    env: { ...process.env, ...(headless ? { WEBCTL_HEADLESS: '1' } : {}) },
  });
  child.unref();
  for (let i = 0; i < 60; i++) {
    await new Promise((r) => setTimeout(r, 500));
    if (await isUp()) return 'started';
  }
  throw new Error('daemon did not come up; see tools/webctl/daemon.log');
}

function out(o) {
  console.log(typeof o === 'string' ? o : JSON.stringify(o, null, 2));
}

const { f, pos } = flags(rest);

try {
  switch (cmd) {
    case 'start':
      out(await startDaemon(!!f.headless));
      break;
    case 'stop':
      out(await api('/shutdown', { method: 'POST' }));
      break;
    case 'status':
      out(await api('/health'));
      break;
    case 'goto':
      out(await api('/goto', { method: 'POST', body: { url: pos[0] } }));
      break;
    case 'reload':
      out(await api('/reload', { method: 'POST', body: {} }));
      break;
    case 'shot': {
      const p = pos[0] ? path.resolve(pos[0]) : '';
      const qs = new URLSearchParams();
      if (p) qs.set('path', p);
      if (f.full) qs.set('full', '1');
      if (f.selector) qs.set('selector', f.selector);
      out(await api('/shot?' + qs.toString()));
      break;
    }
    case 'snapshot': {
      // Capture a golden regression snapshot: screenshot + serialized def (the
      // deterministic oracle) into tools/webctl/baseline/<name>.{png,json}.
      const name = pos[0] || `snap-${Date.now()}`;
      const dir = path.join(__dirname, f.dir || 'baseline');
      fs.mkdirSync(dir, { recursive: true });
      const png = path.join(dir, name + '.png');
      await api('/shot?' + new URLSearchParams({ path: png }).toString());
      const def = await api('/eval', {
        method: 'POST',
        body: { expr: 'JSON.stringify((window.__GP_DEF__&&window.__GP_DEF__())||null)' },
      });
      const jsonPath = path.join(dir, name + '.json');
      fs.writeFileSync(jsonPath, def.result ? JSON.stringify(JSON.parse(def.result), null, 2) : 'null');
      out({ png, def: jsonPath, defNull: !def.result });
      break;
    }
    case 'logs': {
      const qs = new URLSearchParams();
      if (f.since) qs.set('since', f.since);
      if (f.kind) qs.set('kind', f.kind);
      if (f.clear) qs.set('clear', '1');
      const r = await api('/logs?' + qs.toString());
      for (const e of r.logs) {
        const ts = new Date(e.t).toISOString().slice(11, 23);
        console.log(`#${e.id} ${ts} [${e.kind}] ${e.text}${e.stack ? '\n' + e.stack : ''}`);
      }
      console.log(`-- seq=${r.seq} (${r.logs.length} shown)`);
      break;
    }
    case 'click':
      out(await api('/click', { method: 'POST', body: { selector: pos[0] } }));
      break;
    case 'fill':
      out(await api('/fill', { method: 'POST', body: { selector: pos[0], value: pos.slice(1).join(' ') } }));
      break;
    case 'press':
      out(await api('/press', { method: 'POST', body: { selector: pos[0], key: pos[1] } }));
      break;
    case 'wait':
      out(await api('/wait', { method: 'POST', body: { selector: pos[0], state: f.state } }));
      break;
    case 'text':
      out(await api('/text', { method: 'POST', body: { selector: pos[0] } }));
      break;
    case 'eval':
      out(await api('/eval', { method: 'POST', body: { expr: pos.join(' ') } }));
      break;
    default:
      out(
        [
          'webctl <cmd> — drive the visSele WebUI',
          '  start [--headless]       launch the browser daemon (default headed, watchable)',
          '  stop                     shut the daemon + browser down',
          '  status                   daemon health + current url',
          '  goto <url> | reload',
          '  shot [file] [--full] [--selector CSS]   screenshot (png)',
          '  snapshot <name> [--dir D]   golden capture: screenshot + serialized def (regression oracle)',
          '  logs [--since N] [--kind console] [--clear]   console/error/network logs',
          '  click <CSS> | fill <CSS> <val> | press <CSS> <Key> | wait <CSS> [--state]',
          '  text <CSS>               innerText of matches',
          "  eval <js...>             run JS in the page, return result",
        ].join('\n')
      );
  }
} catch (e) {
  console.error('ERR: ' + e.message);
  process.exit(1);
}
