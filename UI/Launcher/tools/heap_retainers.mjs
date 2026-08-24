// Who is holding the detached DOM nodes?
//
//   node tools/heap_retainers.mjs <file.heapsnapshot>
//
// Everything measured so far says the renderer retains DOM that is no longer in
// the document: nodes climb 2700/min, listeners 478/min, a forced collection
// returns none of it, and turning the image stream off stops it dead. What none
// of those instruments can say is WHICH object keeps the reference -- and
// without that, a fix is a guess. Two of them already were.
//
// A V8 heap snapshot has the answer. It is a flat-array format: `nodes` is a
// run of fixed-width records described by meta.node_fields, `edges` likewise,
// and both index into `strings`. Edges are stored FORWARDS (owner -> owned),
// so the retainers of a node have to be found by inverting the whole edge list
// once and then reading it backwards.
import fs from 'node:fs';

const FILE = process.argv[2];
if (!FILE) { console.error('usage: node tools/heap_retainers.mjs <file.heapsnapshot>'); process.exit(2); }

console.log('reading ' + FILE + ' (' + (fs.statSync(FILE).size / 1048576).toFixed(1) + ' MB)');
const snap = JSON.parse(fs.readFileSync(FILE, 'utf8'));

const meta = snap.snapshot.meta;
const NF = meta.node_fields.length;
const EF = meta.edge_fields.length;
const nodeTypes = meta.node_types[0];
const edgeTypes = meta.edge_types[0];

const fNodeType = meta.node_fields.indexOf('type');
const fNodeName = meta.node_fields.indexOf('name');
const fNodeSelf = meta.node_fields.indexOf('self_size');
const fNodeEdgeCount = meta.node_fields.indexOf('edge_count');
const fEdgeType = meta.edge_fields.indexOf('type');
const fEdgeName = meta.edge_fields.indexOf('name_or_index');
const fEdgeTo = meta.edge_fields.indexOf('to_node');

const nodes = snap.nodes, edges = snap.edges, strings = snap.strings;
const nodeCount = nodes.length / NF;
console.log(`${nodeCount} nodes, ${edges.length / EF} edges`);

// Where each node's edges begin, so an edge index can be mapped back to its
// owner without scanning.
const firstEdge = new Uint32Array(nodeCount + 1);
{
  let e = 0;
  for (let i = 0; i < nodeCount; i++) {
    firstEdge[i] = e;
    e += nodes[i * NF + fNodeEdgeCount];
  }
  firstEdge[nodeCount] = e;
}

const nodeName = (i) => strings[nodes[i * NF + fNodeName]];
const nodeType = (i) => nodeTypes[nodes[i * NF + fNodeType]];

// Reverse index, built once: for every edge, remember which node owns it.
const edgeOwner = new Uint32Array(edges.length / EF);
for (let i = 0; i < nodeCount; i++) {
  for (let e = firstEdge[i]; e < firstEdge[i + 1]; e++) edgeOwner[e] = i;
}

// to_node is a BYTE offset into `nodes`, not a node index.
const edgeTarget = (e) => edges[e * EF + fEdgeTo] / NF;

const retainers = new Map();          // node index -> [{owner, edgeName, edgeType}]
for (let e = 0; e < edgeOwner.length; e++) {
  const t = edgeTarget(e);
  let list = retainers.get(t);
  if (!list) retainers.set(t, list = []);
  const et = edgeTypes[edges[e * EF + fEdgeType]];
  // Weak edges do not keep anything alive; counting them as retainers points
  // the finger at the wrong object.
  if (et === 'weak') continue;
  const raw = edges[e * EF + fEdgeName];
  list.push({ owner: edgeOwner[e], name: (et === 'element' || et === 'hidden') ? '[' + raw + ']' : strings[raw], type: et });
}

// The nodes in question: V8 names them "Detached <tag>" once they are out of
// the document but still reachable.
const detached = [];
for (let i = 0; i < nodeCount; i++) {
  const n = nodeName(i);
  if (n.startsWith('Detached ')) detached.push(i);
}
console.log(`\ndetached DOM nodes: ${detached.length}`);
if (!detached.length) {
  console.log('nothing detached is being retained -- either the leak is elsewhere '
            + 'or the snapshot was taken before it accumulated.');
  process.exit(0);
}

const byKind = new Map();
for (const i of detached) {
  const k = nodeName(i);
  byKind.set(k, (byKind.get(k) || 0) + 1);
}
console.log('\nby kind:');
for (const [k, n] of [...byKind].sort((a, b) => b[1] - a[1]).slice(0, 12)) {
  console.log(`  ${String(n).padStart(7)}  ${k}`);
}

// Aggregate the DIRECT retainers. If one object holds thousands of detached
// nodes, it shows up here as a single line with a large count -- which is the
// whole question.
const direct = new Map();
for (const i of detached) {
  for (const r of retainers.get(i) || []) {
    // Ignore parent/child links between detached nodes themselves: a subtree
    // holds itself together, and reporting that says nothing about who is
    // keeping the subtree alive.
    if (nodeName(r.owner).startsWith('Detached ')) continue;
    const key = `${nodeType(r.owner)} ${nodeName(r.owner)}  <--${r.name}`;
    direct.set(key, (direct.get(key) || 0) + 1);
  }
}
console.log('\nwho points AT them (direct retainers, excluding other detached nodes):');
for (const [k, n] of [...direct].sort((a, b) => b[1] - a[1]).slice(0, 20)) {
  console.log(`  ${String(n).padStart(7)}  ${k}`);
}

// And one full path upwards, so the aggregate above has a concrete example.
// Breadth-first from a detached node until a named, non-detached owner turns
// up several levels out.
function pathUp(start, maxDepth = 12) {
  const seen = new Set([start]);
  let frontier = [[start, []]];
  for (let d = 0; d < maxDepth; d++) {
    const next = [];
    for (const [n, path] of frontier) {
      for (const r of retainers.get(n) || []) {
        if (seen.has(r.owner)) continue;
        seen.add(r.owner);
        const step = `${nodeType(r.owner)} ${nodeName(r.owner) || '(anon)'} .${r.name}`;
        const p2 = path.concat(step);
        const nm = nodeName(r.owner);
        if (!nm.startsWith('Detached ') && nm && nm !== 'system' && p2.length > 1) return p2;
        next.push([r.owner, p2]);
      }
    }
    frontier = next;
    if (!frontier.length) break;
  }
  return null;
}

console.log('\nexample retainer chains (leaf first):');
for (const i of detached.slice(-3)) {
  const p = pathUp(i);
  console.log(`  ${nodeName(i)}`);
  if (!p) { console.log('     (no path found within the depth limit)'); continue; }
  for (const step of p) console.log('     <- ' + step);
}
