// Durable local mirror (IndexedDB) of inspection-record DB inserts that have not
// yet been confirmed by the remote traceability DB. Goal: a record must not be
// silently lost if the DB connection drops or the page reloads mid-flight.
//
// Lifecycle (driven by DB_WS in script.jsx):
//   persistPending(record, source) -> seq   when a record is queued for send
//   deletePending(seq)                       when the insert is confirmed (insertOK)
//   getPendingBySource(source)               at startup, to replay orphans left
//                                            over from a previous session/reload
//
// The in-memory consume-queue handles live reconnect retries; this layer only
// adds cross-reload durability. Capped at MAX_RECORDS (oldest dropped) so a long
// outage can't grow unbounded.

const DB_NAME = "visSele_local";
const STORE = "pending_inserts";

// How long an outage this survives, expressed in records.
//
// 1000 was ~45 SECONDS at this bench's 22 parts/s -- and the records are lean
// (stripOverlayOnly runs before the send), about 3.5 kB each, so the old cap
// bought 3.5 MB of safety. IndexedDB is not the constraint here and never was:
// 100k records is ~350 MB and a little over an hour of production at full rate,
// which is the difference between surviving a network blip and surviving
// someone rebooting a switch.
//
// Overridable because the right number depends on the record rate and the disk,
// and neither belongs in this file.
const MAX_RECORDS = Number(
  (typeof window !== "undefined" && window.__INSP_DB_QUEUE_MAX__) || 100000);

// Records this process has DELETED to stay under the cap -- evidence destroyed,
// not evidence delayed. Read by the UI so an operator can see it happening;
// dropping silently is how an outage becomes a hole in the traceability record
// that nobody knows the size of.
let droppedTotal = 0;
export function droppedCount() { return droppedTotal; }

// Records an OPERATOR deliberately deleted, kept apart from droppedTotal.
//
// Both destroy evidence, and that is where the resemblance ends: the cap drops
// the oldest record because it ran out of room, and nobody chose it. This is
// someone standing at the machine deciding that a backlog is not worth sending.
// Folding them into one number would put the cap's explanation ("暫存滿了")
// underneath a deletion nobody's buffer overflowed for, so they count separately
// and the panel says which is which.
let purgedTotal = 0;
export function purgedCount() { return purgedTotal; }

// Delete every not-yet-confirmed record for one source (all sources when the
// argument is omitted). Returns how many went.
//
// A cursor rather than getAll+delete: the cap is 100k records at ~3.5 kB, and
// materialising 350 MB into an array to throw it away is a way to take the tab
// down while clearing a queue that was only ever a nuisance.
export async function deletePendingBySource(source) {
  const db = await openDB();
  const src = (source === undefined) ? undefined : String(source);
  let n = 0;
  await new Promise((resolve, reject) => {
    const cur = store(db, "readwrite").openCursor();
    cur.onsuccess = () => {
      const c = cur.result;
      if (!c) { resolve(); return; }
      if (src === undefined || c.value.source === src) { c.delete(); n++; }
      c.continue();
    };
    cur.onerror = () => reject(cur.error);
  });
  purgedTotal += n;
  if (n > 0) {
    try {
      console.warn("[inspDBQueue] operator DELETED " + n + " unsent record(s) for "
        + (src === undefined ? "ALL sources" : src) + " -- these are gone, not queued");
    } catch { /* a logger must never take the queue with it */ }
  }
  return n;
}

let dbPromise = null;

function openDB() {
  if (dbPromise) return dbPromise;
  dbPromise = new Promise((resolve, reject) => {
    if (typeof indexedDB === "undefined") { reject(new Error("indexedDB unavailable")); return; }
    const req = indexedDB.open(DB_NAME, 2);
    req.onupgradeneeded = () => {
      const db = req.result;
      // v1 used a "failed_inserts" store; v2 is the durable "pending_inserts"
      // mirror. Drop the old store on upgrade so we don't carry stale entries.
      if (db.objectStoreNames.contains("failed_inserts")) db.deleteObjectStore("failed_inserts");
      if (!db.objectStoreNames.contains(STORE)) {
        db.createObjectStore(STORE, { keyPath: "seq", autoIncrement: true });
      }
    };
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
  return dbPromise;
}

function store(db, mode) {
  return db.transaction(STORE, mode).objectStore(STORE);
}

function reqP(request) {
  return new Promise((resolve, reject) => {
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

// Persist a not-yet-confirmed record; returns its auto-assigned seq key.
export async function persistPending(record, source) {
  const db = await openDB();
  const seq = await reqP(store(db, "readwrite").add({ t: Date.now(), source: String(source || ""), record }));
  await trimToCap(db);
  return seq;
}

// Remove a record once its insert is confirmed.
export async function deletePending(seq) {
  if (seq == null) return;
  const db = await openDB();
  await reqP(store(db, "readwrite").delete(seq));
}

// All still-pending records for a given source (DB_WS instance id), oldest first.
export async function getPendingBySource(source) {
  const db = await openDB();
  const all = await reqP(store(db, "readonly").getAll());
  const src = String(source || "");
  return all.filter((e) => e.source === src).map((e) => ({ seq: e.seq, record: e.record }));
}

export async function pendingInsertCount(source) {
  const db = await openDB();
  if (source === undefined) return reqP(store(db, "readonly").count());
  const all = await reqP(store(db, "readonly").getAll());
  const src = String(source);
  return all.filter((e) => e.source === src).length;
}

// Drop oldest entries until at most MAX_RECORDS remain (global cap across sources).
//
// This is the only place in the pipeline that destroys a record rather than
// delaying it, so it says so. It used to delete silently: past the cap the
// oldest inspection results disappeared one per part, and the only thing on
// screen was a "disconnected" banner that looked exactly the same at second 44
// as at second 45. An operator could watch an outage for an hour believing the
// data was waiting for them.
async function trimToCap(db) {
  const count = await reqP(store(db, "readonly").count());
  let toDrop = count - MAX_RECORDS;
  if (toDrop <= 0) return;
  const asked = toDrop;
  await new Promise((resolve, reject) => {
    const cur = store(db, "readwrite").openCursor(); // autoIncrement seq → oldest first
    cur.onsuccess = () => {
      const cursor = cur.result;
      if (cursor && toDrop > 0) { cursor.delete(); toDrop--; cursor.continue(); }
      else resolve();
    };
    cur.onerror = () => reject(cur.error);
  });
  droppedTotal += asked;
  // Rate-limited so a sustained outage does not bury the console, but the FIRST
  // one is always printed: the transition from "buffering" to "losing" is the
  // event worth timestamping.
  if (droppedTotal === asked || droppedTotal % 100 < asked) {
    try {
      console.error("[inspDBQueue] buffer full at " + MAX_RECORDS
        + " -- DISCARDED " + droppedTotal + " oldest inspection record(s); "
        + "these are gone, not queued");
    } catch { /* a logger must never take the queue with it */ }
  }
}
