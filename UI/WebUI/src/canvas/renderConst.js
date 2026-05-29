// Shared canvas render constants. Extracted from EverCheckCanvasComponent.js so
// both renderUTIL and the canvas classes can use them without a circular import.
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { MEASURERSULTRESION } from 'UTIL/InspectionEditorLogic';

export const MEASURE_RESULT_VISUAL_INFO = {
  [MEASURERSULTRESION.UNSET]: { COLOR: "rgba(128,128,128,0.7)", TEXT: MEASURERSULTRESION.UNSET },
  [MEASURERSULTRESION.NA]: { COLOR: "rgba(128,128,128,0.7)", TEXT: MEASURERSULTRESION.NA },
  [MEASURERSULTRESION.UOK]: { COLOR: "rgba(128,200,128,1)", TEXT: MEASURERSULTRESION.UOK },
  [MEASURERSULTRESION.LOK]: { COLOR: "rgba(128,200,128,1)", TEXT: MEASURERSULTRESION.LOK },
  [MEASURERSULTRESION.UCNG]: { COLOR: "rgba(255,255,0,0.7)", TEXT: MEASURERSULTRESION.UCNG },
  [MEASURERSULTRESION.LCNG]: { COLOR: "rgba(255,255,0,0.7)", TEXT: MEASURERSULTRESION.LCNG },
  [MEASURERSULTRESION.USNG]: { COLOR: "rgba(255,0,0,0.7)", TEXT: MEASURERSULTRESION.USNG },
  [MEASURERSULTRESION.LSNG]: { COLOR: "rgba(255,0,0,0.7)", TEXT: MEASURERSULTRESION.LSNG },
};

export const SHAPE_TYPE_COLOR = {
  [SHAPE_TYPE.line]: "hsl(0, 60%, 35%)",
  [SHAPE_TYPE.arc]: "hsl(48, 60%, 35%)",
  [SHAPE_TYPE.search_point]: "hsl(180, 60%, 35%)",

  [SHAPE_TYPE.aux_line]: "hsl(48, 60%, 35%)",
  [SHAPE_TYPE.aux_point]: "hsl(220, 60%, 35%)",
  [SHAPE_TYPE.measure]: "hsl(290, 80%, 65%)",
  default: "rgba(100,50,100)"
};
