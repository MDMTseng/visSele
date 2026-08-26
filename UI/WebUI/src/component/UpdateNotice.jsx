// "There is a newer version waiting; restart when you are ready."
//
// The launcher can install a package and repoint current.json while the machine
// keeps inspecting -- installing only writes a new directory, and the pointer is
// read at start and never again. So the operator is TOLD, and picks the moment.
// Nothing here ever restarts anything.
//
// FAIL SAFE IS THE FIRST REQUIREMENT. This file runs inside the inspection UI,
// which must not care whether it is hosted by a launcher at all. Four things can
// be true and all of them mean "render nothing, forever, silently":
//
//   1. no window.launcher            -- a plain browser, or the Vite dev server
//   2. launcher without updateCheck  -- an older launcher than this WebUI
//   3. the call throws               -- IPC handler absent ("No handler registered")
//   4. the reply is not the shape we expect
//
// Any of them disables this component for the rest of the session rather than
// retrying: a machine whose launcher cannot answer is not a machine that should
// be asked every five minutes, and an error loop in the inspection UI is a far
// worse outcome than a missed update notification.
import React, { useEffect, useRef, useState } from 'react';
import notification from 'antd/lib/notification';
import Button from 'antd/lib/button';
import { mkLog } from 'UTIL/logger';

const log = mkLog('ui.update');

const CHECK_EVERY_MS = 5 * 60 * 1000;

// Nothing is read off the reply until it has been checked. A launcher that
// answers with something unexpected is treated exactly like one that cannot
// answer at all.
function usable(r) {
  return !!r && r.ok === true && (r.available === null || typeof r.available === 'object')
    && (r.pending === null || typeof r.pending === 'string');
}

export default function UpdateNotice() {
  const [state, setState] = useState(null);
  const dead = useRef(false);            // set once, never cleared
  const announced = useRef(null);
  const applying = useRef(false);

  useEffect(() => {
    // 1 and 2: no host, or a host too old to know the question.
    const api = (typeof window !== 'undefined' && window.launcher) || null;
    if (!api || typeof api.updateCheck !== 'function' || typeof api.updateApply !== 'function') {
      log.info('[update] no launcher update API -- update notices disabled');
      dead.current = true;
      return undefined;
    }

    let timer = null;
    let cancelled = false;

    const check = async () => {
      if (dead.current || cancelled) return;
      let r;
      try {
        r = await api.updateCheck();          // 3
      } catch (e) {
        log.info('[update] launcher refused the check -- disabling', { error: String(e && e.message) });
        dead.current = true;
        setState(null);
        return;
      }
      if (!usable(r)) {                       // 4
        log.info('[update] unexpected reply -- disabling', { reply: r });
        dead.current = true;
        setState(null);
        return;
      }
      if (cancelled) return;
      setState(r);
    };

    check();
    timer = setInterval(check, CHECK_EVERY_MS);
    return () => { cancelled = true; if (timer) clearInterval(timer); };
  }, []);

  // Already installed and selected: the only thing left is a restart, and that
  // is the operator's call. Said once per version, not once per poll.
  useEffect(() => {
    if (!state || !state.pending) return;
    if (announced.current === 'pending:' + state.pending) return;
    announced.current = 'pending:' + state.pending;
    notification.success({
      message: `${state.pending} 已就緒`,
      description: `下次重新啟動機台時生效。目前執行中的是 ${state.running || '(未知)'},不受影響。`,
      duration: 0,
      key: 'insp-update',
    });
  }, [state && state.pending]);

  const apply = async (pkg) => {
    if (applying.current) return;
    applying.current = true;
    notification.info({ message: `安裝 ${pkg.version} …`, description: '機台會繼續檢驗,不會中斷。', duration: 0, key: 'insp-update' });
    let r;
    try {
      r = await window.launcher.updateApply(pkg.file);
    } catch (e) {
      r = { ok: false, error: String(e && e.message) };
    }
    applying.current = false;
    if (!r || !r.ok) {
      notification.error({ message: '安裝失敗', description: (r && r.error) || '未知原因', duration: 0, key: 'insp-update' });
      return;
    }
    announced.current = null;              // let the pending notice speak
    notification.destroy('insp-update');
    setState((s) => (s ? { ...s, available: null, current: r.version } : s));
  };

  if (dead.current || !state || !state.available) return null;
  const pkg = state.available;
  if (!pkg.version || !pkg.file) return null;

  // Offered once per version. Dismissing it is allowed -- the next check does
  // not nag, because `announced` remembers.
  if (announced.current !== 'avail:' + pkg.version) {
    announced.current = 'avail:' + pkg.version;
    notification.open({
      message: `有新版本 ${pkg.version}`,
      description: `目前執行中的是 ${state.running || '(未知)'}。安裝不會中斷檢驗,要重新啟動之後才會換版。`,
      duration: 0,
      key: 'insp-update',
      btn: (
        <Button type="primary" size="small" onClick={() => apply(pkg)}>
          安裝,下次重啟時更新
        </Button>
      ),
    });
  }
  return null;
}
