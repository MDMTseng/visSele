'use strict';

const $ = (id) => document.getElementById(id);
const L = window.launcher;

// Text nodes only, everywhere. Core output, error strings and paths chosen by
// the operator all end up on this page, and none of it is trusted markup.
function el(tag, cls, text) {
  const n = document.createElement(tag);
  if (cls) n.className = cls;
  if (text !== undefined) n.textContent = text;
  return n;
}

const MAX_LOG_LINES = 3000;
const logEl = $('log');

// Every backslash escape written into this project through a shell heredoc has
// arrived with the backslash eaten, more than once. A named constant cannot be
// mangled on the way in.
const NL = String.fromCharCode(10);

function appendLog(text, cls) {
  const atBottom = logEl.scrollHeight - logEl.scrollTop - logEl.clientHeight < 40;
  logEl.appendChild(el('div', cls, text));
  while (logEl.childElementCount > MAX_LOG_LINES) logEl.removeChild(logEl.firstChild);
  // Only follow the tail if the operator was already at the tail -- yanking the
  // view away while someone is reading an error is its own small betrayal.
  if (atBottom) logEl.scrollTop = logEl.scrollHeight;
}

// --- banner ------------------------------------------------------------------

function banner(level, title, detail) {
  const b = $('banner');
  b.className = 'banner ' + level;
  b.replaceChildren(el('div', 'title', title));
  if (detail) b.appendChild(el('div', 'detail', detail));
  return b;
}
function hideBanner() { $('banner').className = 'banner hidden'; }

function actionButton(b, label, fn) {
  const btn = el('button', '', label);
  btn.onclick = fn;
  const row = el('div', 'actions');
  row.appendChild(btn);
  b.appendChild(row);
  return btn;
}

async function chooseWorkingDir() {
  const r = await L.pickWorkingDir();
  if (r.ok) appendLog(`工作目錄:${r.workingDir}`, 'lnc');
  refresh();
}
async function chooseAppRoot() {
  const r = await L.pickAppRoot();
  if (r.ok) appendLog(`應用資料夾:${r.appRoot}`, 'lnc');
  refresh();
}

function showReason(reason) {
  if (!reason) return hideBanner();
  switch (reason.kind) {
    case 'no-app': {
      const b = banner('warn', '尚未安裝任何版本',
        '按「安裝更新包…」選一個 zip,安裝後選「設為現行」,再啟動。');
      actionButton(b, '指定應用資料夾…', chooseAppRoot);
      break;
    }
    case 'no-working-dir': {
      const b = banner('warn', '尚未指定工作目錄',
        '這是應用程式「執行時所在」的資料夾 —— 它裡面放的是這台機台的校正、' +
        '配方與快照。\n啟動器絕不會建立、複製或搬動這個資料夾裡的任何東西;' +
        '它屬於機台,不屬於任何一個軟體版本。');
      actionButton(b, '指定工作目錄…', chooseWorkingDir);
      break;
    }
    case 'working-dir-missing': {
      const b = banner('bad', '工作目錄不存在', reason.workingDir);
      actionButton(b, '重新指定…', chooseWorkingDir);
      break;
    }
    case 'boot-failed': {
      banner('bad', '無法讀取這個版本的啟動描述',
             `${reason.message}\n版本資料夾:${reason.appDir}`);
      break;
    }
    case 'unmet-requirements': {
      // The application listed these, not the launcher -- including the reason.
      const lines = reason.unmet.map((r) =>
        `${r.kind === 'file' ? '檔案' : '資料夾'}  ${r.path}${r.why ? '\n    ' + r.why : ''}`);
      const b = banner('bad', '這個版本要求的東西不存在',
        lines.join('\n') + `\n\n工作目錄:${reason.workingDir}`);
      b.appendChild(el('div', 'detail',
        '這份清單來自該版本的 scripts/boot.js —— 啟動器本身不知道應用需要什麼。'));
      actionButton(b, '重新指定工作目錄…', chooseWorkingDir);
      break;
    }
    case 'spawn-failed':
      banner('bad', '無法啟動', reason.error);
      break;
    case 'core-exited': {
      const how = reason.expected
        ? (reason.forced ? '被強制結束(未能在期限內乾淨關機)' : '已正常停止')
        : '意外結束';
      const b = banner(reason.expected ? 'info' : 'bad', `核心${how}`,
        `退出碼 ${reason.code}${reason.signal ? ' / ' + reason.signal : ''}`
        + ` · 執行 ${reason.ranFor.toFixed(1)} 秒`
        + (reason.logFile ? `\n日誌:${reason.logFile}` : ''));
      if (!reason.expected) {
        b.appendChild(el('div', 'detail',
          '不會自動重啟 —— 核心是判定良品與否的那一環,無聲拉起可能讓不良品流過去。' +
          '請看下方最後的輸出,確認原因之後再按「啟動」。'));
      }
      if (Array.isArray(reason.tail) && reason.tail.length) {
        logEl.replaceChildren();
        appendLog(`---- 結束前的最後 ${reason.tail.length} 行 ----`, 'lnc');
        for (const line of reason.tail) appendLog(line, line.startsWith('[err]') ? 'err' : undefined);
      }
      break;
    }
    default:
      hideBanner();
  }
}

// --- rendering ----------------------------------------------------------------

function formatDuration(s) {
  if (s < 90) return `${s.toFixed(0)} 秒`;
  const m = s / 60;
  return m < 90 ? `${m.toFixed(1)} 分` : `${(m / 60).toFixed(1)} 小時`;
}

function renderCore(st, plan) {
  const row = $('coreStatus');
  row.replaceChildren();

  let cls = 'off', word = '未執行';
  if (st.running && st.unresponsive) { cls = 'bad'; word = '無回應'; }
  else if (st.running && st.missedPings > 0) { cls = 'warn'; word = '執行中(有漏 ping)'; }
  else if (st.running) { cls = 'on'; word = '執行中'; }

  const state = el('div', 'kv');
  state.appendChild(el('span', 'dot ' + cls));
  state.appendChild(el('span', 'v', word));
  row.appendChild(state);

  const kv = (k, v) => {
    const d = el('div', 'kv');
    d.appendChild(el('span', 'k', k));
    d.appendChild(el('span', 'v', v));
    row.appendChild(d);
  };
  if (st.pid) kv('主行程 PID', String(st.pid));
  if (st.uptimeS != null) kv('執行時間', formatDuration(st.uptimeS));

  // The health reply is whatever the application's health check returned, so
  // nothing here may assume a shape. Show the fields we recognise if they are
  // there, and say the check passed if they are not -- an application that
  // reports its health differently is not a broken one.
  const h = st.lastHealth;
  if (h) {
    const info = h.info;
    const one = info && !Array.isArray(info) ? info : null;
    if (one && (one.version || one.git)) kv('版本', `${one.version ?? ''} ${one.git ?? ''}`.trim());
    kv('最後回應', `${((Date.now() - h.at) / 1000).toFixed(0)} 秒前`);
  }

  // Only worth saying when more than one process is involved; with a single
  // process the panel above already says everything.
  const procs = st.processes || [];
  if (procs.length > 1) {
    kv('行程', procs.map((p) => `${p.id}${p.running ? '' : ' ✕'}`).join(' · '));
  }
  const noControl = procs.filter((p) => p.running && !p.hasControl);
  if (noControl.length) {
    kv('⚠', `${noControl.map((p) => p.id).join(', ')} 無控制通道,只能強制終止`);
  }
  if (st.lastStopWasForced) kv('上次停止', '強制結束');

  $('btnStart').disabled = st.running || !plan;
  $('btnStop').disabled = !st.running;
}

// The plan, shown verbatim. The whole point of moving this out of the launcher
// is that "what exactly gets run" is now a property of the installed version --
// so it has to be visible, or it has just moved somewhere less inspectable.
function renderPlan(st) {
  const g = $('plan');
  g.replaceChildren();
  if (!st.plan) {
    g.appendChild(el('div', 'note',
      st.planError ? `無法取得啟動描述(${st.planError.kind})` : '尚未有可執行的版本'));
    return;
  }
  const line = (k, v) => {
    g.appendChild(el('div', 'k', k));
    g.appendChild(el('div', 'v', v));
  };
  const p = st.plan;
  if (p.name) line('名稱', p.name);

  for (const pr of p.processes) {
    const label = p.processes.length > 1
      ? `${pr.id}${pr.primary ? '(主)' : ''}` : '執行檔';
    line(label, pr.exe);
    line('　參數', pr.args.length ? pr.args.join(' ') : '(無)');
    line('　工作目錄', pr.cwd);
    if (pr.env.length) line('　環境變數', pr.env.join(', '));
    line('　控制通道', pr.control ? `${pr.control.host}:${pr.control.port}`
                                  : '(無 —— 停止只能強制終止)');
    line('　就緒等待', `${pr.readyTimeoutMs} ms`);
  }

  line('UI', p.ui ? (p.ui.kind === 'url' ? p.ui.target : p.ui.target) : '(這個版本沒有 UI)');
  if (p.requires.length) {
    line('需要', p.requires.map((r) => `${r.path}${r.why ? '  — ' + r.why : ''}`).join(NL));
  }
  // Which behaviours this version overrode. If a machine misbehaves on
  // shutdown, "this version supplies its own requestShutdown" is the first
  // thing worth knowing, and it should not require reading the package.
  line('自訂行為', p.hooks.length ? p.hooks.join(', ') : '(全部使用內建)');

  g.appendChild(el('div', 'k', ''));
  g.appendChild(el('div', 'note',
    `以上全部來自該版本的 ${st.bootRel} —— 啟動器本身不含任何執行檔名稱、參數、埠號或目錄結構。`));
}

function renderVersions(st) {
  const tb = $('versions').querySelector('tbody');
  tb.replaceChildren();
  if (!st.versions.length) {
    const tr = el('tr');
    const td = el('td', 'note', '尚未安裝任何版本');
    td.colSpan = 4;
    tr.appendChild(td);
    tb.appendChild(tr);
  } else {
    for (const v of st.versions) {
      const tr = el('tr');
      tr.appendChild(el('td', 'ver', v.version));

      const tagTd = el('td');
      if (v.current) tagTd.appendChild(el('span', 'tag cur', '現行'));
      if (!v.valid) tagTd.appendChild(el('span', 'tag bad', '不完整'));
      tr.appendChild(tagTd);

      const note = [];
      if (!v.valid && v.missing.length) note.push('缺少 ' + v.missing.join(', '));
      if (v.declaredVersion && v.declaredVersion !== v.version) note.push(`info.json: ${v.declaredVersion}`);
      if (v.installedAt) note.push(new Date(v.installedAt).toLocaleString());
      tr.appendChild(el('td', 'note', note.join(' · ')));

      const act = el('td', 'act');
      if (!v.current && v.valid) {
        const b = el('button', 'ghost small', '設為現行');
        b.disabled = st.core.running;
        b.onclick = async () => {
          try {
            await L.selectVersion(v.version);
            appendLog(`現行版本切換為 ${v.version}`, 'lnc');
            refresh();
          } catch (e) { appendLog('切換失敗:' + e.message, 'err'); }
        };
        act.appendChild(b);
      }
      tr.appendChild(act);
      tb.appendChild(tr);
    }
  }
  $('btnInstall').disabled = st.core.running;
}

function renderSettings(st) {
  const g = $('settings');
  g.replaceChildren();
  const line = (k, v, button, note) => {
    g.appendChild(el('div', 'k', k));
    const cell = el('div', 'v', v == null ? '(未指定)' : String(v));
    if (note) cell.appendChild(el('div', 'note', note));
    g.appendChild(cell);
    g.appendChild(button || el('div'));
  };
  const pick = (label, fn) => {
    const b = el('button', 'ghost small', label);
    b.disabled = st.core.running;
    b.onclick = fn;
    return b;
  };

  line('應用資料夾', st.appRoot, pick('變更…', chooseAppRoot),
       '安裝的各個版本與 current.json 放在這裡。');
  line('工作目錄', st.workingDir, pick('變更…', chooseWorkingDir),
       '應用程式執行時所在的資料夾。機台的校正與配方在這裡 —— 啟動器只讀路徑,'
       + '從不建立、複製或搬動裡面的東西。');
  line('保留版本數', st.config.keepVersions);
  line('關機等待', `${st.config.shutdownTimeoutMs} ms`);
  line('設定檔', st.configFile);
}

// --- wiring ---------------------------------------------------------------------

async function refresh() {
  const st = await L.status();
  $('ident').textContent =
    `launcher ${st.launcherVersion} · electron ${st.electron}\n${st.current ? st.current : 'no version selected'}`;
  renderCore(st.core, st.plan);
  renderPlan(st);
  renderVersions(st);
  renderSettings(st);
  if (st.configError) appendLog(st.configError, 'err');

  if (st.lastExit) showReason({ kind: 'core-exited', ...st.lastExit });
  else if (st.planError) showReason(st.planError);
  else hideBanner();
  return st;
}

$('btnStart').onclick = async () => {
  hideBanner();
  logEl.replaceChildren();
  $('btnStart').disabled = true;
  const r = await L.startCore();
  if (!r.ok) appendLog('啟動失敗:' + r.error, 'err');
  refresh();
};

$('btnStop').onclick = async () => {
  $('btnStop').disabled = true;
  const r = await L.stopCore();
  appendLog(r.forced ? '未在期限內結束,已強制終止' : '已停止', r.forced ? 'err' : 'lnc');
  refresh();
};

$('btnInstall').onclick = async () => {
  const r = await L.chooseAndInstall();
  if (r.canceled) return;
  if (!r.ok) appendLog('安裝失敗:' + r.error, 'err');
  refresh();
};

$('btnLogs').onclick = () => L.openFolder('logs');

L.onLog(({ message }) => appendLog(message, 'lnc'));
L.onCoreLine((line) => appendLog(line, line.startsWith('[err]') ? 'err' : undefined));
L.onHealth(() => refresh());
L.onReason((reason) => showReason(reason));

refresh();
setInterval(refresh, 5000);
