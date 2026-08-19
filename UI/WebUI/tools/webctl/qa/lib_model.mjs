// The two things every core-dependent qa suite needs, each of which had been
// copy-pasted into 22 files with no import between them.
//
// 1. WHICH DEF TO LOAD. The old default was an absolute path on one developer's
//    Mac. Off that machine the load simply failed -- and then, because of (2),
//    the suite blamed the core for it. Measured 2026-08-19: 21 of 39 suites
//    "skipped" this way, ~44s of retries each, 15 of the run's 16 minutes.
//
// 2. WHAT A LOAD FAILURE MEANS. "The core is down" and "the def is not there"
//    are different problems in different subsystems, and the old test could not
//    tell them apart because it only ever saw the error STRING:
//
//        /not connected|timeout|did not load|reconnect|ECONNREF/i
//
//    A missing def produces "did not load", which matches -- so an absent
//    fixture was reported as a dead core, with the core up and answering on
//    4090 the whole time. That is a wrong diagnosis pointed at the wrong
//    subsystem, which is worse than no diagnosis.
//
// Deliberately NOT exported here: reset(). It exists in 24-line and 27-line
// variants across the suites and the differences have not been audited.
// Unifying it is a separate change with its own risk.
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));

// The checked-in fixture. flows.mjs, cycle.mjs and enter_inspection.mjs already
// default to it; these suites were the last holdouts on the Mac path.
export const MODEL_PATH = process.env.WEBCTL_MODEL ||
  path.join(here, '..', 'fixtures', 'caliper_verify_tagged');

// Kept verbatim from the 23 copies: the shape of the message has not changed,
// only what we do after matching it.
export const looksLikeLoadFailure = (msg) =>
  /not connected|timeout|did not load|reconnect|ECONNREF/i.test(String(msg || ''));

// Ask the PAGE whether the core is there, rather than inferring it from the
// text of an error. `ev` is the suite's own eval helper.
// Path per r1_comm.mjs T2: ConnInfo.CORE_ID_CONN_INFO.type === 'WS_CONNECTED'.
export async function coreIsUp(ev) {
  try {
    return (await ev(`(function(){
      try{
        var ci = window.__GP_STORE__ && window.__GP_STORE__.getState().ConnInfo;
        var core = ci && ci.CORE_ID_CONN_INFO;
        return core ? String(core.type) : 'no-conn-info';
      }catch(e){ return 'threw'; }
    })()`)) === 'WS_CONNECTED';
  } catch { return false; }
}

// Turn a failed load into a message that names the right subsystem.
// Returns { kind: 'no-core' | 'no-model' | 'other', msg }.
export async function diagnoseLoadFailure(ev, lastErr) {
  const detail = String(lastErr || 'timeout');
  if (!looksLikeLoadFailure(detail)) return { kind: 'other', msg: detail };
  if (await coreIsUp(ev)) {
    return {
      kind: 'no-model',
      msg: `NO-MODEL: core is UP (WS_CONNECTED) but the def did not load: ${detail}`
         + ` -- tried ${MODEL_PATH}. Set WEBCTL_MODEL to a def that exists.`,
    };
  }
  return { kind: 'no-core', msg: `CORE-DOWN: ${detail}` };
}
