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
    case 'setup-requested': {
      banner('info', '設定模式',
        '核心已停止,現在可以更改路徑、切換版本或安裝更新包。'
        + '完成後按「啟動」,或關閉視窗。');
      break;
    }
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

// EVERY ACTION THAT CHANGES WHAT RUNS GOES THROUGH HERE.
//
// Asking at the moment of the click, rather than gating the buttons, is the
// point: a disabled button with a padlock somewhere else makes people hunt for
// the unlock and then click everything while it is open. This way the prompt
// arrives attached to the thing it is protecting, and it says which thing.
//
// Unlocking lasts the launcher run, so a sequence -- install, then select --
// asks once. Closing the launcher relocks.
let unlockedNow = false;
function withUnlock(what, run) {
  if (unlockedNow) return run();
  return new Promise((resolve) => {
    let word = '';
    openModal('需要密碼', (b) => {
      b.appendChild(el('div', 'note', `${what} 會改變下次啟動執行的版本。`));
      b.appendChild(el('div', 'note', '這只是防止誤觸,不是安全機制。'));
      const inp = document.createElement('input');
      inp.type = 'password';
      inp.className = 'modalInput';
      inp.placeholder = '密碼';
      inp.oninput = () => { word = inp.value; };
      inp.onkeydown = (e) => { if (e.key === 'Enter') go(); };
      b.appendChild(inp);
      setTimeout(() => inp.focus(), 0);
    }, [
      { label: '取消', onClick: () => { closeModal(); resolve(undefined); } },
      { label: '確定', primary: true, onClick: () => go() },
    ]);
    async function go() {
      const r = await L.unlock(word);
      if (!r.ok) { appendLog(r.error || '密碼不對', 'err'); return; }
      unlockedNow = true;
      closeModal();
      resolve(await run());
    }
  });
}

// ---------------------------------------------------------------- modals --
//
// Built here rather than sitting in index.html because both are transient and
// carry data: leaving an empty dialog in the markup means a second place that
// has to be kept in step with what fills it.
function closeModal() {
  const m = document.getElementById('modal');
  if (m) m.remove();
}

function openModal(title, buildBody, actions) {
  closeModal();
  const back = el('div');
  back.id = 'modal';
  back.className = 'modalBack';
  const box = el('div', 'modalBox');
  box.appendChild(el('h2', 'modalTitle', title));
  const body = el('div', 'modalBody');
  buildBody(body);
  box.appendChild(body);
  const foot = el('div', 'modalFoot');
  for (const a of actions) {
    const b = el('button', a.primary ? 'primary' : 'ghost', a.label);
    b.onclick = a.onClick;
    foot.appendChild(b);
  }
  box.appendChild(foot);
  back.appendChild(box);
  // Click outside = dismiss, same as 稍後再說: the question comes back next
  // start unless it was skipped, so closing it is never destructive.
  back.onclick = (e) => { if (e.target === back) closeModal(); };
  document.body.appendChild(back);
}

// WHY A VERSION DID NOT TAKE. Opened from the "!" on its row.
function showFailure(version, fail) {
  openModal(`${version}${fail.refused ? ' 不適用於這台機器' : ' 安裝後置作業失敗'}`, (b) => {
    b.appendChild(el('div', 'modalReason', fail.reason || '(沒有留下原因)'));
    const meta = [];
    if (fail.at) meta.push(new Date(fail.at).toLocaleString());
    if (fail.tries) meta.push(`已嘗試 ${fail.tries} 次`);
    if (meta.length) b.appendChild(el('div', 'note', meta.join(' · ')));
    b.appendChild(el('div', 'note', fail.refused
      ? '這是套件本身拒絕,不是安裝出錯。重試不會成功 —— 需要另一個套件。'
      : '版本已安裝並通過驗證,只是機器還沒準備好,所以沒有被啟用。修好原因後可以重試。'));
  }, [{ label: '關閉', primary: true, onClick: closeModal }]);
}

// THE UPDATE QUESTION. Raised at start-up when release.json points somewhere
// else; the answer is always the operator's.
function showUpdateOffer(offer) {
  if (!offer || !offer.version) return;
  let skip = false;
  openModal('有新的版本可以安裝', (b) => {
    b.appendChild(el('div', 'modalReason',
      `${offer.current || '(未指定)'}  →  ${offer.version}`));
    b.appendChild(el('div', 'note', offer.file));
    b.appendChild(el('div', 'note',
      '安裝後下次重新啟動時才生效,不會中斷正在進行的檢測。'));
    if (offer.failure) {
      b.appendChild(el('div', 'note err',
        `這一版先前試過並失敗:${offer.failure.reason || '(沒有留下原因)'}`));
    }
    // SKIP IS PART OF THE ANSWER, not a separate menu somewhere. Without it the
    // same question returns at every start and people learn to dismiss it
    // without reading -- which is how a real update gets missed later.
    const row = el('label', 'modalSkip');
    const cb = document.createElement('input');
    cb.type = 'checkbox';
    cb.onchange = () => { skip = cb.checked; };
    row.appendChild(cb);
    row.appendChild(el('span', null, `跳過這一版,不要再問 ${offer.version}`));
    b.appendChild(row);
  }, [
    { label: '稍後再說', onClick: async () => {
        if (skip) { try { await L.setSkipped(offer.version, true); } catch (e) { /* shown in log */ } }
        closeModal(); refresh();
      } },
    { label: '安裝', primary: true, onClick: async () => {
        closeModal();
        appendLog(`安裝 ${offer.file} …`, 'lnc');
        try {
          const r = await withUnlock('安裝 ' + offer.version, () => L.installFromSource(offer.file));
          if (r === undefined) { refresh(); return; }   // cancelled
          if (!r.ok) { appendLog('安裝失敗:' + r.error, 'err'); refresh(); return; }
          // Installed is not selected. Pointing at it is the second half, and
          // it is what makes the next start use it.
          const sel = await L.retrySetup(offer.version);   // already unlocked by the install above
          appendLog(sel.ok ? `${offer.version} 已設為現行 -- 下次重新啟動時生效`
                           : '安裝後置作業未完成:' + sel.error, sel.ok ? 'lnc' : 'err');
        } catch (e) { appendLog('安裝失敗:' + e.message, 'err'); }
        refresh();
      } },
  ]);
}

function renderUpdates(st) {
  const g = $('updateSrc');
  g.replaceChildren();
  const u = st.update || { source: null, packages: [], release: null };

  const pickBtn = el('button', 'ghost small', u.source ? '變更…' : '指定…');
  pickBtn.disabled = st.core.running;
  pickBtn.onclick = async () => {
    try { const r = await L.pickUpdateSource(); if (r && r.ok) refresh(); }
    catch (e) { appendLog('設定更新來源失敗:' + e.message, 'err'); }
  };
  g.appendChild(el('div', 'k', '資料夾'));
  const cell = el('div', 'v', u.source || '(未指定)');
  if (u.source) {
    cell.appendChild(el('div', 'note',
      '唯讀。啟動器只從這裡讀取,不會寫入或刪除 —— 這是同步資料夾,在這裡刪掉的東西會傳播到全公司。'));
  }
  if (u.error) cell.appendChild(el('div', 'note err', u.error === 'missing' ? '這個資料夾不存在' : u.error));
  g.appendChild(cell);
  g.appendChild(pickBtn);

  // CHECK, not apply. The label has to carry that distinction, because
  // "automatic update" is read as "it might restart the machine at any time"
  // and that is the one thing this never does.
  g.appendChild(el('div', 'k', '啟動時檢查'));
  const chk = el('div', 'v', st.checkUpdates ? '開' : '關');
  chk.appendChild(el('div', 'note', st.checkUpdates
    ? '開啟時若 release.json 指向別的版本，會跳出詢問。裝不裝是你決定。'
    : '不檢查，也不會詢問。這張表仍然可以手動安裝。'));
  g.appendChild(chk);
  const chkBtn = el('button', 'ghost small', st.checkUpdates ? '關閉' : '開啟');
  chkBtn.disabled = st.core.running;
  chkBtn.onclick = async () => {
    chkBtn.disabled = true;
    try { await L.setCheckUpdates(!st.checkUpdates); refresh(); }
    catch (e) { appendLog('設定失敗：' + e.message, 'err'); refresh(); }
  };
  g.appendChild(chkBtn);

  const tb = $('updatePkgs').querySelector('tbody');
  tb.replaceChildren();
  if (!u.source) return;
  if (!u.packages.length) {
    const tr = el('tr'); const td = el('td', 'note', '這個資料夾裡沒有可用的更新包');
    td.colSpan = 4; tr.appendChild(td); tb.appendChild(tr); return;
  }
  for (const p of u.packages) {
    const tr = el('tr');
    tr.appendChild(el('td', 'ver', p.version));
    const tags = el('td');
    if (p.wanted) tags.appendChild(el('span', 'tag good', 'release 指定'));
    if (p.current) tags.appendChild(el('span', 'tag cur', '現行'));
    else if (p.installed) tags.appendChild(el('span', 'tag prev', '已安裝'));
    tr.appendChild(tags);
    tr.appendChild(el('td', 'note', p.file));
    const act = el('td', 'act');
    const b = el('button', 'ghost small', p.installed ? '重新安裝' : '安裝');
    b.disabled = st.core.running;
    b.onclick = async () => {
      b.disabled = true;
      appendLog(`安裝 ${p.file} …`, 'lnc');
      try {
        const r = await withUnlock('安裝 ' + p.version, () => L.installFromSource(p.file));
        if (r === undefined) { b.disabled = false; return; }   // cancelled
        if (!r.ok) appendLog('安裝失敗:' + r.error, 'err');
        refresh();
      } catch (e) { appendLog('安裝失敗:' + e.message, 'err'); refresh(); }
    };
    act.appendChild(b);
    tr.appendChild(act);
    tb.appendChild(tr);
  }
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
      if (st.lastGood && st.lastGood.version === v.version) tagTd.appendChild(el('span', 'tag good', '已驗證'));
      if (st.previous === v.version) tagTd.appendChild(el('span', 'tag prev', '前一版'));
      if (!v.valid) tagTd.appendChild(el('span', 'tag bad', '不完整'));
      // A VERSION THAT WAS TRIED AND DID NOT TAKE.
      //
      // Hung off the row rather than shown as one global 'last failure',
      // because the list is what somebody browses and the question they have
      // is about the row under the cursor. Click for the reason -- the mark
      // says there is something to know, the detail says what.
      const fail = (st.updateState && st.updateState.failures)
        ? st.updateState.failures[v.version] : null;
      if (fail) {
        const bang = el('span', 'tag bad', fail.refused ? '! 不適用' : '! 安裝後失敗');
        bang.style.cursor = 'pointer';
        bang.title = '點選看詳細';
        bang.onclick = () => showFailure(v.version, fail);
        tagTd.appendChild(bang);
      }
      if (st.updateState && st.updateState.skipped.includes(v.version)) {
        tagTd.appendChild(el('span', 'tag prev', '已跳過'));
      }
      tr.appendChild(tagTd);

      const note = [];
      if (!v.valid && v.missing.length) note.push('缺少 ' + v.missing.join(', '));
      if (v.declaredVersion && v.declaredVersion !== v.version) note.push(`info.json: ${v.declaredVersion}`);
      if (st.lastGood && st.lastGood.version === v.version)
        note.push(`連續執行過 ${(st.lastGood.ranForS / 3600).toFixed(1)} 小時`);
      if (v.installedAt) note.push(new Date(v.installedAt).toLocaleString());
      tr.appendChild(el('td', 'note', note.join(' · ')));

      const act = el('td', 'act');
      // Retry is manual and only offered where it can work: a refusal is the
      // package saying 'not this machine', and asking again cannot change it.
      if (fail && !fail.refused && v.valid) {
        const rb = el('button', 'ghost small', '重試安裝後作業');
        rb.disabled = st.core.running;
        rb.onclick = async () => {
          rb.disabled = true;
          appendLog(`重試 ${v.version} 的安裝後置作業…`, 'lnc');
          try {
            const r = await withUnlock('重試 ' + v.version, () => L.retrySetup(v.version));
            if (r === undefined) { rb.disabled = false; return; }   // cancelled
            appendLog(r.ok ? `${r.version} 完成,下次重新啟動時生效`
                           : '仍然失敗:' + r.error, r.ok ? 'lnc' : 'err');
          } catch (e) { appendLog('重試失敗:' + e.message, 'err'); }
          refresh();
        };
        act.appendChild(rb);
      }
      if (!v.current && v.valid) {
        const b = el('button', 'ghost small', '設為現行');
        b.disabled = st.core.running;
        b.onclick = async () => {
          try {
            const done = await withUnlock('切換到 ' + v.version, () => L.selectVersion(v.version));
            if (done === undefined) return;   // cancelled
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

  line('更新來源', st.update && st.update.source, pick('變更…', async () => {
    try { const r = await L.pickUpdateSource(); if (r && r.ok) refresh(); }
    catch (e) { appendLog('設定更新來源失敗:' + e.message, 'err'); }
  }), '更新包來的地方,唯讀。通常在同步資料夾裡,例如 data/sync/DEV/<機種>/update。');
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
  renderUpdates(st);
  renderVersions(st);
  renderSettings(st);
  if (st.configError) appendLog(st.configError, 'err');
  // A path arrangement that lets one job of the launcher destroy another's data
  // is not a log line. It is the loudest thing on the screen until it is fixed.
  for (const p of (st.pathIssues || [])) appendLog('路徑設定危險:' + p.detail, 'err');

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
// The launcher pushes the question once, at start-up, after the shell has
// loaded. Not polled: it is a question, not a state, and asking twice is how
// you get two dialogs.
L.onUpdateOffer((offer) => showUpdateOffer(offer));
L.onCoreLine((line) => appendLog(line, line.startsWith('[err]') ? 'err' : undefined));
L.onHealth(() => refresh());
// --- the three-tap setup gate ------------------------------------------------
//
// While the core boots, this screen is what is on the display. Three taps
// anywhere on it during that window stop the start and keep the launcher up --
// the only way into the settings on a machine with no keyboard.
//
// Taps must be close together to count, so that three unrelated prods over the
// course of a minute do not add up to a request nobody made.
const TAP_WINDOW_MS = 900;
const TAPS_NEEDED = 3;

let gateDeadline = 0;
let gateTimer = null;
let taps = [];

function gateOpen() { return gateDeadline > Date.now(); }

function paintGate() {
  if (!gateOpen()) {
    clearInterval(gateTimer); gateTimer = null;
    if ($('gate')) $('gate').className = 'gate hidden';
    return;
  }
  const left = Math.ceil((gateDeadline - Date.now()) / 1000);
  const got = taps.length;
  $('gate').className = 'gate';
  $('gate').replaceChildren(
    el('div', 'gate-main', `啟動中… ${left} 秒`),
    el('div', 'gate-hint', got
      ? `再點 ${TAPS_NEEDED - got} 下進入設定模式`
      : `連點三下進入設定模式`));
}

L.onSetupGate((info) => {
  if (!info || !info.ms) { gateDeadline = 0; taps = []; paintGate(); return; }
  gateDeadline = Date.now() + info.ms;
  taps = [];
  if (!gateTimer) gateTimer = setInterval(paintGate, 100);
  paintGate();
});

async function onGateTap() {
  if (!gateOpen()) return;
  const now = Date.now();
  taps = taps.filter((t) => now - t < TAP_WINDOW_MS);
  taps.push(now);
  if (taps.length < TAPS_NEEDED) { paintGate(); return; }
  gateDeadline = 0; taps = [];
  paintGate();
  const r = await L.requestSetup();
  if (!r || !r.ok) appendLog(`[launcher] 設定模式未開啟:${(r && r.error) || '未知原因'}`, 'err');
}
// Capture phase, so a tap on a button counts too -- the operator is tapping the
// screen, not aiming at anything.
document.addEventListener('pointerdown', onGateTap, true);

L.onReason((reason) => showReason(reason));

refresh();
setInterval(refresh, 5000);
