// Turn a running soak's CSV into a page somebody can actually read.
//
//   node soak_report.mjs [soak-log] [out.html]
//     defaults: /tmp/soak6h_12000.log  ->  <scratch>/soak_report.html
//
// A soak prints one CSV row a minute for six hours. That is the right format
// for the run and the wrong one for a person: the question during a run is
// never "what was column 24 at minute 173", it is "is anything drifting, and
// does it need me now". The columns that answer that are three lines on one
// time axis (the memories), one on another (the rates), and a handful of
// counters that must stay at zero.
//
// The data is EMBEDDED, not fetched: the page is a snapshot, and a snapshot
// that silently shows stale numbers is worse than one that says when it was
// taken. Re-run this and republish to move it forward.
import fs from 'node:fs';
import path from 'node:path';

// The default is where Git Bash's /tmp actually lives on this machine: a
// literal "/tmp/..." handed to node on Windows resolves to a C:\tmp that does
// not exist, so the shell and the script disagree about the same path.
// New run, new log. The 12000 run's file stays where it is.
// THREE RUNS, NOT ONE.
//
// A single run's traces answer "is this one healthy". The question on the bench
// is now comparative -- 12000 stopped on a lost camera clock, 10000 did not, and
// between them the camera and light offsets moved -- and that cannot be read off
// three separate pages. Each run keeps its own log and its own header, because
// the columns changed between them (miss_delta arrived after the 12000 run), so
// every file is parsed against ITS OWN header rather than a shared assumption.
const RUNS = [
  { id: '12000', label: 'Plate 12000',
    note: 'stopped at 136 min on error 13, camera clock lost',
    log: 'C:/Users/w2110/AppData/Local/Temp/soak6h_12000.log' },
  { id: '10000a', label: '10000, old offsets',
    note: 'CAM1 9515 / L1A 9494 — 85% of parts got no verdict',
    log: 'C:/Users/w2110/AppData/Local/Temp/soak6h_10000.log' },
  { id: '10000b', label: '10000, new offsets',
    note: 'CAM1 8010 / L1A 8000 — stopped at 86 min on error 2, a part reached the switch with no verdict',
    log: 'C:/Users/w2110/AppData/Local/Temp/soak6h_10000b.log' },
];
const OUT = process.argv[2] || path.join(
  'C:/Users/w2110/AppData/Local/Temp/claude/C--Users-w2110-Documents-workspace-visSele',
  '8b8b78b7-1ef0-4c98-8245-47dba9add707/scratchpad/soak_report.html');

function readRun(run) {
  let raw;
  try { raw = fs.readFileSync(run.log, 'utf8').split(/\r?\n/); }
  catch { return null; }
  const headLine = raw.find((l) => l.startsWith('t_min,'));
  if (!headLine) return null;
  const cols = headLine.split(',');

  // The panel column is a quoted string full of commas, so quotes are honoured
  // rather than splitting blindly.
  const parseRow = (line) => {
    const out = []; let cur = '', q = false;
    for (const ch of line) {
      if (ch === '"') { q = !q; continue; }
      if (ch === ',' && !q) { out.push(cur); cur = ''; continue; }
      cur += ch;
    }
    out.push(cur);
    const o = {};
    cols.forEach((c, i) => {
      const v = out[i];
      o[c] = (v === undefined || v === '') ? null
           : (c === 'panel' || c === 'err') ? v
           : Number.isNaN(Number(v)) ? v : Number(v);
    });
    return o;
  };

  const rows = raw.filter((l) => /^[0-9]/.test(l) && l.includes(','))
                  .map(parseRow).filter((r) => r.t_min !== null);
  const setup = raw.filter((l) => /^\[\d/.test(l) || /^ {4}/.test(l)).slice(0, 12);
  const done = raw.some((l) => l.startsWith('soak done'));
  const st = fs.statSync(run.log);
  return { ...run, rows, setupLines: setup, done,
           takenLocal: st.mtime.toLocaleString('sv-SE').slice(0, 16) };
}

const runs = RUNS.map(readRun).filter(Boolean);
if (!runs.length) throw new Error('no soak logs found');
const data = { runs, targetMin: 360,
               takenLocal: new Date().toLocaleString('sv-SE').slice(0, 16) };

const here = path.dirname(new URL(import.meta.url).pathname.replace(/^\/(?=[A-Za-z]:)/, ''));
const html = fs.readFileSync(path.join(here, 'soak_report.template.html'), 'utf8')
  .replace('/*__DATA__*/null', JSON.stringify(data));
fs.writeFileSync(OUT, html);
console.log(OUT);
for (const r of runs)
  console.log(`  ${r.id.padEnd(8)} ${String(r.rows.length).padStart(4)} samples  `
            + `t=${r.rows.length ? r.rows[r.rows.length - 1].t_min : 0} min`
            + (r.done ? '  (finished)' : ''));
