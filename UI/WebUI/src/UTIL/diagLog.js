// In-memory diagnostics ring buffer for field debugging without devtools.
//
// initDiag() wraps the console methods so EVERY log line (raw console.* AND
// loglevel, which ultimately calls console) is captured with a timestamp into a
// capped ring (oldest dropped). On a deployed touchscreen with no devtools the
// operator can hit "Download Diagnostics" to hand the developer a log file.
// The global window error / unhandledrejection handlers (script.jsx) log via
// console.error, so they are captured here automatically.

const MAX_ENTRIES = 2000;
let ring = [];
let installed = false;
let origConsole = {};

function safeArg(a) {
  if (a instanceof Error) return a.stack || (a.name + ": " + a.message);
  if (typeof a === "string") return a;
  try { return JSON.stringify(a); }
  catch (e) { try { return String(a); } catch (e2) { return "[unserializable]"; } }
}

function pushEntry(level, args) {
  let msg;
  try { msg = Array.prototype.map.call(args, safeArg).join(" "); }
  catch (e) { msg = "[diag: failed to format args]"; }
  ring.push({ t: Date.now(), level, msg });
  if (ring.length > MAX_ENTRIES) ring.shift();
}

export function initDiag() {
  if (installed || typeof console === "undefined") return;
  installed = true;
  ["log", "info", "warn", "error", "debug"].forEach((level) => {
    const orig = console[level] ? console[level].bind(console) : function () {};
    origConsole[level] = orig;
    console[level] = function () {
      pushEntry(level, arguments);   // capture first; never call console.* here (recursion)
      orig.apply(null, arguments);
    };
  });
  pushEntry("info", ["[diag] capture started"]);
}

export function diagText() {
  const head =
    "=== visSele WebUI diagnostics ===\n" +
    "generated: " + new Date().toISOString() + "\n" +
    "userAgent: " + (typeof navigator !== "undefined" ? navigator.userAgent : "?") + "\n" +
    "entries: " + ring.length + " (cap " + MAX_ENTRIES + ")\n" +
    "=================================\n";
  const body = ring
    .map((e) => new Date(e.t).toISOString() + " [" + e.level + "] " + e.msg)
    .join("\n");
  return head + body + "\n";
}

export function diagCount() { return ring.length; }

// Trigger a browser download of the current buffer as a text file.
export function downloadDiag() {
  try {
    const blob = new Blob([diagText()], { type: "text/plain" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    const stamp = new Date().toISOString().replace(/[:.]/g, "-");
    a.href = url;
    a.download = "visSele_diag_" + stamp + ".txt";
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  } catch (e) {
    origConsole.error && origConsole.error("downloadDiag failed", e);
  }
}
