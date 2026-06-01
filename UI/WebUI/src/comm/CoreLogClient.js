// CoreLogClient — WebUI ↔ inspd_log.v1 WebSocket transport.
//
// Wraps the protocol from InspectionCore/docs/LOGGING_WEBUI.md.  Owns the
// reconnect-with-backoff lifecycle and exposes a single event surface so the
// React UI can stay dumb.
//
// Lifecycle:
//   1. WebSocket(url, 'inspd_log.v1')  — fails fast if server rejects the
//      subprotocol.  We don't downgrade.
//   2. Server sends `hello` immediately.  We auto-send the persisted
//      `subscribe` filters as soon as it arrives so live tail starts without
//      the consumer doing anything.
//   3. Backlog frames stream first, then live `log` / `log_batch`.
//   4. `crash` → final frame for this connection.  We mark it terminal and
//      DON'T auto-reconnect (the producer died; the drainer follows).
//   5. close 1011 / any other close → exponential backoff reconnect
//      (1→2→4→8→cap at 30s) unless the consumer called .stop().
//
// Public API (event subscriptions via .on(event, fn) → returns unsub fn):
//   on('hello',     (hello) => ...)       once per connection
//   on('log',       (entry) => ...)       fires for live + backlog entries
//   on('backlog',   (entries) => ...)     batched delivery of backlog_chunk
//   on('modules',   (modules) => ...)     reply to getModules()
//   on('crash',     (crash) => ...)       producer crash; terminal
//   on('status',    ({state,attempt,err}) => ...)
//                                          state: 'idle' | 'connecting'
//                                                  | 'connected' | 'crashed'
//                                                  | 'closed'    | 'reconnecting'
//
// Commands:
//   client.subscribe({ min_level, modules, include_ephemeral, backlog })
//   client.setLevel({ scope, module?, level })   // returns Promise<ack>
//   client.getModules()                          // returns Promise<modules[]>
//   client.dumpNow()                             // returns Promise<{ok,dump_path}>
//   client.ping()                                // returns Promise<pong>
//
// All commands return promises keyed off our internal `id` counter; the
// server's `ack` / `modules` reply with `id` echoed back resolves it.

import { mkLog } from 'UTIL/logger';

const log = mkLog('comm.corelog');

const DEFAULT_BACKOFF_MS = [1000, 2000, 4000, 8000, 16000, 30000];
const PING_INTERVAL_MS   = 20_000;
const COMMAND_TIMEOUT_MS = 5000;

export class CoreLogClient {
  constructor({ url = 'ws://127.0.0.1:4091/log', subprotocol = 'inspd_log.v1',
                backoff = DEFAULT_BACKOFF_MS } = {}) {
    this.url = url;
    this.subprotocol = subprotocol;
    this.backoff = backoff;
    this.ws = null;
    this.attempt = 0;
    this.state = 'idle';
    this._stopped = false;
    this._listeners = {};       // event → Set<fn>
    this._pending  = new Map();  // id → { resolve, reject, timer }
    this._idSeq = 0;
    this._subscription = null;   // remembered to re-apply on reconnect
    this._pingTimer = null;
  }

  // ─── Event subscription ────────────────────────────────────────────────
  on(event, fn) {
    if (!this._listeners[event]) this._listeners[event] = new Set();
    this._listeners[event].add(fn);
    return () => this._listeners[event].delete(fn);
  }
  _emit(event, payload) {
    const set = this._listeners[event];
    if (!set) return;
    for (const fn of set) {
      try { fn(payload); }
      catch (e) { log.error('[listener-threw]', { event, err: String(e) }); }
    }
  }

  // ─── Lifecycle ─────────────────────────────────────────────────────────
  start() {
    if (this.ws && this.ws.readyState <= 1) return;
    this._stopped = false;
    this._connect();
  }
  stop() {
    this._stopped = true;
    this._setState('closed');
    if (this._pingTimer) { clearInterval(this._pingTimer); this._pingTimer = null; }
    if (this.ws) {
      try { this.ws.close(1000, 'client stop'); } catch (_) {}
      this.ws = null;
    }
    for (const [, p] of this._pending) p.reject(new Error('client stopped'));
    this._pending.clear();
  }

  _setState(state, extra) {
    this.state = state;
    this._emit('status', { state, attempt: this.attempt, ...extra });
  }

  _connect() {
    this._setState('connecting');
    let ws;
    try {
      ws = new WebSocket(this.url, this.subprotocol);
    } catch (e) {
      log.error('[ctor]', { url: this.url, err: String(e) });
      this._scheduleReconnect();
      return;
    }
    this.ws = ws;

    ws.onopen = () => {
      this.attempt = 0;
      log.info('[open]', { url: this.url, subprotocol: ws.protocol });
      this._setState('connected');
      // hello arrives next — wait for it before subscribing.
      this._pingTimer = setInterval(() => { this.ping().catch(() => {}); }, PING_INTERVAL_MS);
    };

    ws.onmessage = (ev) => {
      let msg;
      try { msg = JSON.parse(typeof ev.data === 'string' ? ev.data : new TextDecoder().decode(ev.data)); }
      catch (e) { log.warn('[parse-fail]', { len: ev.data && ev.data.length }); return; }
      this._handle(msg);
    };

    ws.onerror = () => {
      log.warn('[error-event]', { url: this.url });
    };

    ws.onclose = (ev) => {
      if (this._pingTimer) { clearInterval(this._pingTimer); this._pingTimer = null; }
      const meta = { code: ev && ev.code, reason: ev && ev.reason };
      // 1011 = drainer dying (parent dead).  Stop reconnect attempts here only
      // if we just received a crash frame; otherwise treat as a normal close
      // and back off.  The doc says the producer can restart without the
      // drainer noticing, so reconnect is the safe default.
      if (this.state === 'crashed') { log.info('[close-after-crash]', meta); return; }
      log.info('[close]', meta);
      this.ws = null;
      if (!this._stopped) this._scheduleReconnect();
    };
  }

  _scheduleReconnect() {
    if (this._stopped) return;
    const delay = this.backoff[Math.min(this.attempt, this.backoff.length - 1)];
    this.attempt++;
    this._setState('reconnecting', { delay });
    log.info('[reconnect-scheduled]', { attempt: this.attempt, delay });
    setTimeout(() => { if (!this._stopped) this._connect(); }, delay);
  }

  // ─── Frame handling ────────────────────────────────────────────────────
  _handle(msg) {
    switch (msg.type) {
      case 'hello':
        log.info('[hello]', msg);
        this._emit('hello', msg);
        // Re-apply persisted subscription, if any.
        if (this._subscription) this._sendSubscribe(this._subscription);
        break;
      case 'log':
        this._emit('log', msg);
        break;
      case 'logBatch':
        if (Array.isArray(msg.items)) for (const e of msg.items) this._emit('log', e);
        break;
      case 'backlogChunk':
        this._emit('backlog', { items: msg.items || [], more: !!msg.more });
        break;
      case 'dropped':
        // Backpressure: drainer had to discard N buffered lines for our
        // connection. Forward to the UI so it can render a gap marker.
        this._emit('dropped', { count: msg.count || 0, sinceUnixNano: msg.sinceUnixNano || null });
        break;
      case 'crash':
        log.warn('[crash]', { signal: msg.signal, timeUnixNano: msg.timeUnixNano, dumpPath: msg.dumpPath });
        this._setState('crashed', { crash: msg });
        this._emit('crash', msg);
        break;
      case 'modules':
        this._resolvePending(msg.id, msg.modules);
        break;
      case 'ack':
        if (msg.ok) this._resolvePending(msg.id, msg);
        else this._rejectPending(msg.id, msg.error || 'ack failed');
        break;
      case 'pong':
        this._resolvePending(msg.id, msg);
        break;
      default:
        log.debug('[unknown-frame]', { type: msg.type });
    }
  }

  // ─── Commands ──────────────────────────────────────────────────────────
  _nextId() { return 'c' + (++this._idSeq); }

  _resolvePending(id, value) {
    const p = this._pending.get(id);
    if (!p) return;
    clearTimeout(p.timer);
    this._pending.delete(id);
    p.resolve(value);
  }
  _rejectPending(id, err) {
    const p = this._pending.get(id);
    if (!p) return;
    clearTimeout(p.timer);
    this._pending.delete(id);
    p.reject(new Error(err));
  }

  _send(payload, awaitId) {
    if (!this.ws || this.ws.readyState !== 1) return Promise.reject(new Error('not connected'));
    const id = awaitId || payload.id;
    let promise = Promise.resolve();
    if (id) {
      promise = new Promise((resolve, reject) => {
        const timer = setTimeout(() => {
          this._pending.delete(id);
          reject(new Error('command timeout'));
        }, COMMAND_TIMEOUT_MS);
        this._pending.set(id, { resolve, reject, timer });
      });
    }
    try { this.ws.send(JSON.stringify(payload)); }
    catch (e) { if (id) this._rejectPending(id, String(e)); return Promise.reject(e); }
    return promise;
  }

  // ─── Commands (OTel-aligned schema) ────────────────────────────────────
  //
  // Severity is on the OTel scale:
  //   TRACE=1, DEBUG=5, INFO=9, WARN=13, ERROR=17, FATAL=21
  // Gaps reserved for OTel variants (INFO2=10, ERROR3=19, ...).

  subscribe(opts) {
    // Remember so we can re-apply after reconnect.
    this._subscription = {
      minSeverityNumber: opts.minSeverityNumber ?? 9, // INFO
      modules: opts.modules ?? null,
      includeEphemeral: !!opts.includeEphemeral,
      backlog: opts.backlog ?? { tailN: 500 },
    };
    if (this.state === 'connected') return this._sendSubscribe(this._subscription);
    // Will be sent on next 'hello'.
    return Promise.resolve();
  }
  _sendSubscribe(sub) {
    const id = this._nextId();
    return this._send({
      type: 'subscribe', id,
      minSeverityNumber: sub.minSeverityNumber,
      modules: sub.modules || undefined,
      includeEphemeral: sub.includeEphemeral,
      backlog: sub.backlog,
    }, id);
  }

  setLevel({ scope, module, severityNumber }) {
    const id = this._nextId();
    return this._send({ type: 'setLevel', id, scope, module, severityNumber }, id);
  }

  getModules() {
    const id = this._nextId();
    return this._send({ type: 'getModules', id }, id);
  }

  dumpNow() {
    const id = this._nextId();
    return this._send({ type: 'dumpNow', id }, id);
  }

  ping() {
    const id = this._nextId();
    return this._send({ type: 'ping', id }, id);
  }
}
