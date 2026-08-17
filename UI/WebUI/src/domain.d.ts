/**
 * Core domain contracts for the visSele WebUI (1st-gen).
 *
 * Progressive-TS seed: ONE cohesive, growable file (by design — we prefer this over
 * many tiny type files). It is intentionally PARTIAL — only the fields we rely on are
 * typed; shapes/defs are passed to the core wholesale, so extra fields are allowed via
 * index signatures. Grow it opportunistically when typing helps a debug/refactor.
 *
 * Apply to JS via JSDoc, e.g.:
 *   /** @type {import('./domain').Shape[]} *\/
 * or author new files in .ts/.tsx.
 */

export interface Point { x: number; y: number; }

export type ShapeType =
  | 'line' | 'arc' | 'circle' | 'search_point' | 'measure' | 'aux_line' | 'aux_point';

/** A reference from one shape to another (join by id). */
export interface ShapeRef { id?: number; element?: string; keyTrace?: (string | number)[]; }

/** line/arc caliper locating sub-schema (present when locating === "caliper"). */
export interface CaliperDef { count?: number; width?: number; length?: number; step?: number; }
export interface EdgeDef {
  method?: 'strongest' | 'first' | 'last' | 'middle' | 'nth';
  polarity?: 'any' | 'rising' | 'falling';
  nth?: number;
  min_strength?: number;
}

export interface ShapeBase {
  id: number;
  name?: string;
  type: ShapeType;
  ref?: ShapeRef[];
  ref_baseLine?: ShapeRef;
  [k: string]: unknown; // passthrough — def reaches the core verbatim
}
export interface LineShape extends ShapeBase {
  type: 'line'; pt1: Point; pt2: Point;
  vertex_touch_searching?: boolean;
  locating?: 'contour' | 'caliper';
  caliper?: CaliperDef; edge?: EdgeDef;
}
export interface ArcShape extends ShapeBase {
  type: 'arc'; pt1: Point; pt2: Point; pt3: Point;
  locating?: 'contour' | 'caliper'; caliper?: CaliperDef; edge?: EdgeDef;
}
export interface CircleShape extends ShapeBase { type: 'circle'; }
export interface SearchPointShape extends ShapeBase {
  type: 'search_point'; search_far?: boolean; locating_anchor?: boolean; anchor_corner?: boolean; line_thickness_value?: number;
}
export interface MeasureShape extends ShapeBase {
  type: 'measure'; subtype?: string;
  value_A?: number; value_B?: number; value_X?: number; value_Y?: number;
  USL?: number; LSL?: number; UCL?: number; LCL?: number;
  quality_essential?: boolean; orientation_essential?: boolean; NGasNA?: boolean; NAasNG?: boolean;
}
export interface AuxLineShape extends ShapeBase { type: 'aux_line'; }
export interface AuxPointShape extends ShapeBase { type: 'aux_point'; }

export type Shape =
  | LineShape | ArcShape | CircleShape | SearchPointShape | MeasureShape | AuxLineShape | AuxPointShape;

/** The sig360_circle_line feature (what GenerateFeature_sig360_circle_line produces). */
export interface Sig360Feature {
  type: 'sig360_circle_line';
  ver: string;
  unit: 'px';
  mmpp: number;
  cam_param: Record<string, unknown>;
  features: Shape[];
  inherentfeatures: unknown[];
  [k: string]: unknown;
}

/** The .hydef file root. */
export interface HyDef {
  type: 'binary_processing_group';
  featureSet: Sig360Feature[];
  featureSet_sha1?: string;
  featureSet_sha1_pre?: string;
  featureSet_sha1_root?: string;
  name?: string;
  tag?: string | string[];
  [k: string]: unknown;
}

/** INSPECTION_STATUS enum values (subset). */
export type InspectionStatus = 0 | -1 | -128 | number; // SUCCESS=0, FAILURE=-1, NA=-128

export interface DetectedLine {
  id: number; status: InspectionStatus; cx: number; cy: number; vx: number; vy: number;
  s: number; matching_pts?: number; confidence?: number; pt1?: Point; pt2?: Point;
}
export interface DetectedCircle {
  id: number; status: InspectionStatus; x: number; y: number; r: number;
  s: number; matching_pts?: number; confidence?: number;
}
export interface JudgeReport { id: number; status: InspectionStatus; value: number; detailStatus?: string; name?: string; }
export interface Report {
  detectedLines: DetectedLine[];
  detectedCircles: DetectedCircle[];
  searchPoints: { id: number; status: InspectionStatus; x: number; y: number }[];
  judgeReports: JudgeReport[];
  rotate?: number; cx?: number; cy?: number; isFlipped?: boolean; mmpp?: number;
}

/**
 * The editor model instance. shapeList is the single source of truth for shapes
 * (no separate edit_info.list mirror — collapsed). Partial — add methods as typed.
 */
export interface InspectionEditorLogic {
  shapeList: Shape[];
  inherentShapeList: unknown[];
  sig360info: { reports: { mmpp: number; cam_param: unknown; [k: string]: unknown }[] } | null;
  GenerateFeature_sig360_circle_line(): Sig360Feature;
  SetShape(shape: Partial<Shape> | null, id?: number): Shape | null;
  SetShapeList(list: Shape[]): void;
  FindShapeIdx(id: number, list?: Shape[]): number | undefined;
  applyEditTarSubstate(editInfo: EditInfo, substate: string): EditInfo;
  [k: string]: unknown;
}

/**
 * edit_info — intentionally kept as one cohesive object (per project preference,
 * not fragmented into many slices). Holds the model (_obj), editor selection
 * (edit_tar_*), def metadata, inspection results, and matching params.
 */
export interface EditInfo {
  _obj: InspectionEditorLogic;
  edit_tar_info: (Partial<Shape> & Record<string, unknown>) | null;
  edit_tar_ele_trace: (string | number)[] | null;
  edit_tar_ele_cand: unknown | null;
  inherentShapeList: unknown[];
  __decorator: { list_id_order: number[]; extra_info: unknown[]; control_margin_info: Record<string, unknown> };
  defModelPath?: string;
  DefFileName: string; DefFileTag: string[]; DefFileHash?: string;
  matching_angle_margin_deg: number; matching_angle_offset_deg: number; matching_face: number;
  img: unknown;
  inspReport?: unknown;
  reportStatisticState?: unknown;
  sig360info?: unknown;
  [k: string]: unknown;
}

/** Redux store shape (InspData slice was removed — only UIData + ConnInfo). */
export interface RootState {
  UIData: {
    edit_info: EditInfo;
    c_state: { value: Record<string, string> } | null;
    p_state: unknown; sm: unknown;
    defConf_lock_level: number;
    DICT: Record<string, unknown>;
    System_Setting: Record<string, unknown>;
    machine_custom_setting: Record<string, unknown>;
    [k: string]: unknown;
  };
  ConnInfo: {
    CORE_ID: string;
    [k: string]: unknown;
  };
}
