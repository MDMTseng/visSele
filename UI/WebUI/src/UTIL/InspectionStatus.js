// Leaf enum extracted from BPG_Protocol.js to break a circular import:
// UIAct used to import INSPECTION_STATUS from BPG_Protocol, and BPG_Protocol
// imports `* as UIAct` — that loop hurt build determinism and ESM evaluation
// order. Both now import from this leaf instead.
export const INSPECTION_STATUS = {
  NA: -128,
  UNSET: -100,
  SUCCESS: 0,
  FAILURE: -1,
};
