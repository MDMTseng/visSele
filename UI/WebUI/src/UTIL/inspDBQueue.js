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
const MAX_RECORDS = 1000;

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
async function trimToCap(db) {
  const count = await reqP(store(db, "readonly").count());
  let toDrop = count - MAX_RECORDS;
  if (toDrop <= 0) return;
  await new Promise((resolve, reject) => {
    const cur = store(db, "readwrite").openCursor(); // autoIncrement seq → oldest first
    cur.onsuccess = () => {
      const cursor = cur.result;
      if (cursor && toDrop > 0) { cursor.delete(); toDrop--; cursor.continue(); }
      else resolve();
    };
    cur.onerror = () => reject(cur.error);
  });
}
