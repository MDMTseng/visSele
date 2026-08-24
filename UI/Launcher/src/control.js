// Client for an application's control socket (line-delimited JSON).
//
// The host and port are NOT configured here and NOT hard-coded: they come from
// the application's own scripts/boot.js, which is the only thing that knows
// what channel its executable opens.
//
// Two commands, both defined in ControlSocketThread in wiringPanel.cpp:
//   {"type":"ping"}      -> {"type":"pong","uptime_s":..,"version":..,...}
//   {"type":"shutdown"}  -> {"type":"shutdown","ack":true}, then teardown
//
// One connection per command. The socket is cheap, the commands are rare, and
// a persistent connection would have to carry its own reconnect logic for no
// gain -- while also occupying a slot during the exact window where a health
// check matters most.
'use strict';

const net = require('node:net');

function command(port, obj, timeoutMs = 1500, host = '127.0.0.1') {
  return new Promise((resolve) => {
    const sock = net.connect(port, host);
    let buf = '';
    let settled = false;

    const finish = (result) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      try { sock.destroy(); } catch { /* already gone */ }
      resolve(result);
    };

    const timer = setTimeout(() => finish({ ok: false, error: 'timeout' }), timeoutMs);

    sock.on('connect', () => sock.write(JSON.stringify(obj) + '\n'));
    sock.on('data', (d) => {
      buf += d.toString('latin1');
      const nl = buf.indexOf('\n');
      if (nl < 0) return;
      const line = buf.slice(0, nl);
      try {
        finish({ ok: true, reply: JSON.parse(line) });
      } catch (e) {
        finish({ ok: false, error: `unparsable reply: ${line.slice(0, 120)}` });
      }
    });
    // ECONNREFUSED is the normal answer when the core is not up yet, or has
    // already gone. It is not an error worth decorating.
    sock.on('error', (e) => finish({ ok: false, error: e.code || e.message }));
    sock.on('close', () => finish({ ok: false, error: 'closed with no reply' }));
  });
}

const ping = (port, timeoutMs, host) => command(port, { type: 'ping' }, timeoutMs, host);
const shutdown = (port, timeoutMs, host) => command(port, { type: 'shutdown' }, timeoutMs, host);

module.exports = { command, ping, shutdown };
