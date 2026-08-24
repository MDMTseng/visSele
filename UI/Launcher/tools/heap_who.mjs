// What is this heap object, and who is holding it?
//
//   node --max-old-space-size=6144 tools/heap_who.mjs <file.heapsnapshot> <NodeName> [samples]
//
// heap_retainers.mjs looked for nodes named "Detached ..." and found none: this
// Chromium labels a DOM wrapper that has left the document with its plain class
// name, exactly like one still in it. The counts were there all along under
// HTMLDivElement / HTMLSpanElement / Text -- the filter was wrong, not the data.
//
// So this takes a name and answers the two questions that actually matter:
// what does the object look like (its own property names identify a minified
// class better than its mangled name ever will), and which objects point at it.
import fs from 'node:fs';

const FILE = process.argv[2];
const WANT = process.argv[3];
const SAMPLES = Number(process.argv[4] || 3);
if (!FILE || !WANT) {
  console.error('usage: node tools/heap_who.mjs <file.heapsnapshot> <NodeName> [samples]');
  process.exit(2);
}

const snap = JSON.parse(fs.readFileSync(FILE, 'utf8'));
const meta = snap.snapshot.meta;
const NF = meta.node_fields.length, EF = meta.edge_fields.length;
const nodeTypes = meta.node_types[0], edgeTypes = meta.edge_types[0];
const fT = meta.node_fields.indexOf('type');
const fN = meta.node_fields.indexOf('name');
const fEC = meta.node_fields.indexOf('edge_count');
const eT = meta.edge_fields.indexOf('type');
const eN = meta.edge_fields.indexOf('name_or_index');
const eTo = meta.edge_fields.indexOf('to_node');

const nodes = snap.nodes, edges = snap.edges, S = snap.strings;
const N = nodes.length / NF;
const name = (i) => S[nodes[i * NF + fN]];
const type = (i) => nodeTypes[nodes[i * NF + fT]];

const first = new Uint32Array(N + 1);
for (let i = 0, e = 0; i < N; i++) { first[i] = e; e += nodes[i * NF + fEC]; first[i + 1] = e; }

const owner = new Uint32Array(edges.length / EF);
for (let i = 0; i < N; i++) for (let e = first[i]; e < first[i + 1]; e++) owner[e] = i;

const edgeName = (e) => {
  const t = edgeTypes[edges[e * EF + eT]];
  const raw = edges[e * EF + eN];
  return (t === 'element' || t === 'hidden') ? '[' + raw + ']' : S[raw];
};
const target = (e) => edges[e * EF + eTo] / NF;

const hits = [];
for (let i = 0; i < N; i++) if (name(i) === WANT) hits.push(i);
console.log(`${WANT}: ${hits.length} instances`);
if (!hits.length) process.exit(0);

// What it looks like from the inside.
console.log('\nshape (outgoing property names of one instance):');
{
  const i = hits[Math.floor(hits.length / 2)];
  const props = [];
  for (let e = first[i]; e < first[i + 1]; e++) {
    const t = edgeTypes[edges[e * EF + eT]];
    if (t !== 'property' && t !== 'internal') continue;
    props.push(edgeName(e) + ':' + name(target(e)));
  }
  console.log('  ' + (props.slice(0, 40).join('  ') || '(none)'));
}

// Who points at them, aggregated. One object holding thousands shows up as a
// single line with a large count -- which is the whole question.
const back = new Map();
for (let e = 0; e < owner.length; e++) {
  const t = target(e);
  if (name(t) !== WANT) continue;
  if (edgeTypes[edges[e * EF + eT]] === 'weak') continue;
  const key = `${type(owner[e])} ${name(owner[e]) || '(anon)'}  <--.${edgeName(e)}`;
  back.set(key, (back.get(key) || 0) + 1);
}
console.log('\nretained by (direct, aggregated):');
for (const [k, n] of [...back].sort((a, b) => b[1] - a[1]).slice(0, 20)) {
  console.log(`  ${String(n).padStart(7)}  ${k}`);
}

// And a concrete chain upwards, because an aggregate of minified names is only
// half an answer.
function up(start, maxDepth = 10) {
  const seen = new Set([start]);
  let frontier = [[start, []]];
  for (let d = 0; d < maxDepth; d++) {
    const next = [];
    for (const [n0, path] of frontier) {
      for (let e = 0; e < owner.length; e++) { /* placeholder */ }
      break;
    }
    break;
  }
  return null;
}

// A reverse adjacency list, built once, so chains are cheap to walk.
const rev = new Map();
for (let e = 0; e < owner.length; e++) {
  if (edgeTypes[edges[e * EF + eT]] === 'weak') continue;
  const t = target(e);
  let a = rev.get(t);
  if (!a) rev.set(t, a = []);
  if (a.length < 8) a.push(e);            // a few retainers per node is plenty
}

function chain(start, maxDepth = 9) {
  const seen = new Set([start]);
  let frontier = [[start, []]];
  for (let d = 0; d < maxDepth; d++) {
    const next = [];
    for (const [n0, path] of frontier) {
      for (const e of rev.get(n0) || []) {
        const o = owner[e];
        if (seen.has(o)) continue;
        seen.add(o);
        const step = `${type(o)} ${name(o) || '(anon)'} .${edgeName(e)}`;
        const p2 = path.concat(step);
        const nm = name(o);
        // Stop at something a human can act on: a named non-minified object, a
        // Window, a document, or a React root.
        if (/Window|Document|HTML|Detached|Fiber|root|Array$/i.test(nm) && p2.length >= 2) return p2;
        next.push([o, p2]);
      }
    }
    frontier = next;
    if (!frontier.length) break;
  }
  return null;
}

console.log('\nexample chains upward:');
for (const i of hits.slice(-SAMPLES)) {
  const c = chain(i);
  console.log(`  ${WANT} #${i}`);
  if (!c) { console.log('     (no anchor found within depth)'); continue; }
  for (const s of c) console.log('     <- ' + s);
}
