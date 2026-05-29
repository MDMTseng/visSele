// Local IndexedDB buffer for inspection-record DB writes that failed to persist
// to the remote traceability DB. Instead of silently dropping a failed insert
// (the old empty .catch), we stash it here so it isn't lost. Capped at the most
// recent MAX_RECORDS; when full, the oldest is dropped (ring semantics).
//
// Scope: enqueue + cap + count. Draining/retry-on-reconnect is a deliberate
// follow-up — the immediate goal is "don't silently lose records".

const DB_NAME = "visSele_local";
const STORE = "failed_inserts";
const MAX_RECORDS = 1000;

let dbPromise = null;

function openDB() {
  if (dbPromise) return dbPromise;
  dbPromise = new Promise((resolve, reject) => {
    if (typeof indexedDB === "undefined") { reject(new Error("indexedDB unavailable")); return; }
    const req = indexedDB.open(DB_NAME, 1);
    req.onupgradeneeded = () => {
      const db = req.result;
      if (!db.objectStoreNames.contains(STORE)) {
        db.createObjectStore(STORE, { keyPath: "seq", autoIncrement: true });
      }
    };
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
  return dbPromise;
}

function tx(db, mode) {
  return db.transaction(STORE, mode).objectStore(STORE);
}

// Persist a failed inspection record. Returns a promise (fire-and-forget OK).
export async function enqueueFailedInsert(record, reason) {
  const db = await openDB();
  await new Promise((resolve, reject) => {
    const store = tx(db, "readwrite");
    const req = store.add({ t: Date.now(), reason: String(reason || ""), record });
    req.onsuccess = resolve;
    req.onerror = () => reject(req.error);
  });
  await trimToCap(db);
}

// Drop oldest entries until at most MAX_RECORDS remain.
async function trimToCap(db) {
  const count = await new Promise((resolve, reject) => {
    const req = tx(db, "readonly").count();
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
  let toDrop = count - MAX_RECORDS;
  if (toDrop <= 0) return;
  await new Promise((resolve, reject) => {
    const store = tx(db, "readwrite");
    const cur = store.openCursor(); // keyPath is autoIncrement seq → oldest first
    cur.onsuccess = () => {
      const cursor = cur.result;
      if (cursor && toDrop > 0) { cursor.delete(); toDrop--; cursor.continue(); }
      else resolve();
    };
    cur.onerror = () => reject(cur.error);
  });
}

export async function pendingInsertCount() {
  const db = await openDB();
  return new Promise((resolve, reject) => {
    const req = tx(db, "readonly").count();
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}
