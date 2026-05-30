// Leaf enum extracted from InspectionEditorLogic.js to break a circular import
// cycle (shapes → canvas/renderConst → InspectionEditorLogic → shapes), similar
// to the INSPECTION_STATUS leaf split. Both renderConst and the legacy
// InspectionEditorLogic now import this single leaf — no cycle.
export const MEASURERSULTRESION = {
  UNSET: 'UNSET',
  NA:    'NA',
  UOK:   'UOK',
  LOK:   'LOK',
  OK:    'OK',

  UCNG:  'UCNG',
  LCNG:  'LCNG',
  CNG:   'CNG',

  USNG:  'USNG',
  LSNG:  'LSNG',
  SNG:   'SNG',
  NG:    'NG',
};
