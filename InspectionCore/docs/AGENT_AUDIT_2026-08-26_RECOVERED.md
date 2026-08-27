# Agent audit findings, 2026-08-26 — RECOVERED, UNVERIFIED

Ten agents, **61 numbered findings**, 185 distinct `file:line` references.

## Read this before acting on any of it

**Nothing here has been verified, and the hit rate on what HAS been checked is
bad.** Eight findings were spot-checked across two sessions and **not one was
correct as written**:

| checked | outcome |
|---|---|
| *"a NaN measurement is judged SUCCESS"* | false — the guard is at `FeatureManager_sig360_circle_line.cpp:926` |
| *"20 crash dumps caused by divide-by-zero"* | false — 9 PRODUCER_DIED, 1 SIGSEGV, **0 SIGFPE** |
| F1 / F2 (memory corruption) | real bugs, **wrong file** — they were in `dbg_printf`/`msg_printf` |
| F3 (SEL valve latches until power cycle) | not supported by the code; every SEL-blocking path has a route back |
| X8 (id collision threshold) | off by 10x on its own number — 100000, not 10000 |

Several named something real while pointing at the wrong file, the wrong
threshold or the wrong mechanism. So an entry here is **a search term with a
plausible story attached**, not a defect. Re-derive it from the code before it
goes anywhere near a fix or a backlog.

The agents' own `## CONFIRMED` headings mean *the agent believed it*. Three of
them say so. That is not evidence.

## Why this file exists

These were produced during the 2026-08-26 session and never written down. Four
documents referred to "roughly 75 findings, recorded but unverified" — nothing
in the repo contained them and nothing in git history ever had. They were
recovered from the session transcript on 2026-08-27, at which point they existed
only inside a 20 MB `.jsonl` that the next compaction would have taken.

Recovered verbatim. Nothing has been edited, ranked or filtered, deliberately —
a summary of unverified claims is a second layer of unverified claims.

---

## Trace localization polygon path

`task-id afe1db56b0a711b00`  21154 chars

## 1. AUTHORING (UI)

**Shape type strings**: `'loc_include'` / `'loc_exclude'` (registry constants in `C:\Users\w2110\Documents\workspace\visSele\UI\WebUI\src\redux\actions\UIAct.js:53-54`):
```js
  loc_include:"loc_include",
  loc_exclude:"loc_exclude",
```

**Shape modules** — `C:\Users\w2110\Documents\workspace\visSele\UI\WebUI\src\shapes\loc_include.js` (16 lines) and `loc_exclude.js`, both thin wrappers over `_locRegionCommon.jsx`:
```js
export const type = 'loc_include';
export function applyDefaults(shape) { return applyDefaultsRegion(shape); }
export const draw = makeDraw('#00c853');           // green = include
export const PropertySheet = makePropertySheet('loc_include');
```
`loc_exclude.js:8,14,15` is identical with `'loc_exclude'` and stroke `'#ff5252'`.

Registered at `C:\Users\w2110\Documents\workspace\visSele\UI\WebUI\src\shapes\index.js:23-28`:
```js
export const SHAPE_REGISTRY = { line, arc, search_point, measure, aux_point, aux_line, loc_include, loc_exclude, loc_reg, obj_detect };
```

**The shared region shape implementation** — `C:\Users\w2110\Documents\workspace\visSele\UI\WebUI\src\shapes\_locRegionCommon.jsx`:
- `applyDefaultsRegion` (`:15-18`) — a region is only `shape.points = []`, an ordered list; no other geometry fields.
- `makeDraw(stroke)` (`:33-57`) — polygon outline, closes at &gt;=3 verts, rubber-bands to `shape._cursor` while in progress, `renderer.drawpoint` per vertex.
- `makePropertySheet(kindLabel)` (`:65-98`) — name, point count, "clear all points", and a per-vertex x/y `NumberField` list gated at `PER_POINT_EDIT_MAX = 40` (`:63`).
- Header comment `:4-8` states the invariant you need: *"Both are authored as ordinary shapes (object-frame mm, exactly like every other shape: shape.points[i] is in the same frame as a line's pt1)."*

**Primary authoring surface: SBMStudio** — `C:\Users\w2110\Documents\workspace\visSele\UI\WebUI\src\SBMStudio.jsx`

Toolbar (`:314-320`):
```jsx
      &lt;Divider orientation="left" style={{ margin: '8px 0 4px' }}&gt;特徵範圍 regions&lt;/Divider&gt;
      &lt;TBtn id="include"&gt;＋ include 生成區（{nIncl}）&lt;/TBtn&gt;
      &lt;TBtn id="exclude"&gt;－ exclude 避免區（{nExcl}）&lt;/TBtn&gt;
      &lt;div style={{ display: 'flex', gap: 4 }}&gt;
        &lt;Button size="small" onClick={() =&gt; delLast('loc_include')}&gt;刪last include&lt;/Button&gt;
        &lt;Button size="small" onClick={() =&gt; delLast('loc_exclude')}&gt;刪last exclude&lt;/Button&gt;
```
Counts at `:303-304`; `delLast` at `:299-302` dispatches `Shape_Set({shape:null,id})`.

Mouse logic `ctrlScene` (`:173-189`) — click to add vertex, click within ~12px of vertex 0 with &gt;=3 verts to close:
```js
  if (tool === 'include' || tool === 'exclude') {
    work.cursor = { x: g.mouseOnCanvas.x, y: g.mouseOnCanvas.y };
    if (g.mouseEdge &amp;&amp; st.status === 0) {                 // mouse-up edge
      const movedPx = Math.hypot(st.x - st.px, st.y - st.py);
      if (movedPx &lt; 6) {                                  // a click, not a pan-drag
        const p = { x: g.mouseOnCanvas.x, y: g.mouseOnCanvas.y };
        if (work.poly.length &gt;= 3) {
          const f = work.poly[0];
          const closeW = 12 / scale;                       // ~12px in world units
          if (Math.hypot(p.x - f.x, p.y - f.y) &lt; closeW) {
            onPoly(tool === 'include' ? 'loc_include' : 'loc_exclude', work.poly.slice());
```

Commit into the features array (`:223-229`) — this is the only write path; it goes into `obj.shapeList` (i.e. `features[]`):
```js
  const onPoly = useCallback((type, pts) =&gt; {
    dispatch(DefConfAct.Shape_Set({
      shape: { type, points: pts.map((p) =&gt; ({ x: p.x, y: p.y })),
               name: type === 'loc_include' ? '@__LOC_INCLUDE__' : '@__LOC_EXCLUDE__' },
      id: undefined,
    }));
  }, [dispatch]);
```
Studio rendering of committed regions, `drawScene` `:100-109` (filled green/red).

**Secondary (main canvas) authoring path** — `C:\Users\w2110\Documents\workspace\visSele\UI\WebUI\src\EverCheckCanvasComponent.js:2568-2594`, state `DEFCONF_MODE_LOC_INCLUDE_CREATE` / `..._EXCLUDE_CREATE`:
```js
          const ptype = (this.state.substate == UI_SM_STATES.DEFCONF_MODE_LOC_INCLUDE_CREATE)
            ? SHAPE_TYPE.loc_include : SHAPE_TYPE.loc_exclude;
          if (this.EditShape == null || this.EditShape.type !== ptype) {
            this.EditShape = { type: ptype, points: [], color: this.colorSet.unselected };
          }
          this.EditShape._cursor = { x: mouseOnCanvas2.x, y: mouseOnCanvas2.y };  // rubber-band preview
          if (this.mouseStatus.status == 1 &amp;&amp; ifOnMouseLeftClickEdge) {
            const pts = this.EditShape.points;
            const closeDist = this.mouse_close_dist / this.camera.GetCameraScale();
            if (pts.length &gt;= 3 &amp;&amp;
                Math.hypot(mouseOnCanvas2.x - pts[0].x, mouseOnCanvas2.y - pts[0].y) &lt; closeDist) {
              delete this.EditShape._cursor;
              this.SetShape(this.EditShape);
```
Plus `AvailableShapeFilter` mapping at `:2123-2128`. State machine wiring: `UIAct.js:15-16` (`DEFCONF_MODE_LOC_INCLUDE_CREATE`), `UIAct.js:111-112` (events `Loc_Include_Create`/`Loc_Exclude_Create`), `redux/redux.js:30-31` and `:54-58`.

**Note for your fence feature**: nothing in the app currently *dispatches* `Loc_Include_Create` — grep over `UI/WebUI/src` outside `redux/` finds zero emitters, so this main-canvas path is dormant and SBMStudio is the live authoring UI.

## 2. DEF SERIALIZATION (UI)

`C:\Users\w2110\Documents\workspace\visSele\UI\WebUI\src\UTIL\MISC_Util.js`, inside `defFileGeneration`.

**Step A — unconditional strip from `features[]`** (`:349-356`); the comment block `:333-348` explains that leaving them in kills the whole def (`feature[7] has unknown type:[loc_include]` → `cJSON parse failed`):
```js
  const _featsAll = Array.isArray(report.featureSet[0].features)
    ? report.featureSet[0].features : [];
  const _locIncl = _featsAll.filter((s) =&gt; s &amp;&amp; s.type === 'loc_include');
  const _locExcl = _featsAll.filter((s) =&gt; s &amp;&amp; s.type === 'loc_exclude');
  if (_locIncl.length || _locExcl.length) {
    report.featureSet[0].features = _featsAll.filter(
      (s) =&gt; s &amp;&amp; s.type !== 'loc_include' &amp;&amp; s.type !== 'loc_exclude');
  }
```

**Step B — emit def keys, only under `locating_engine === 'shape_based'`** (`:358`, body `:384-408`):
```js
    const inclShapes = _locIncl;
    const exclShapes = _locExcl;
    const toPolys = (shapes) =&gt; shapes
      .map((s) =&gt; (Array.isArray(s.points) ? s.points.map((p) =&gt; ({ x: p.x, y: p.y })) : []))
      .filter((p) =&gt; p.length &gt;= 3);
    const inclPolys = toPolys(inclShapes);
    const exclPolys = toPolys(exclShapes);

    if (inclPolys.length) {
      report.featureSet[0].localization_include = inclPolys;
    } else {
      const inh = report.featureSet[0].inherentfeatures;
      const sig = inh &amp;&amp; inh[0] &amp;&amp; inh[0].signature;
      if (sig &amp;&amp; Array.isArray(sig.magnitude) &amp;&amp; Array.isArray(sig.angle)) {
        const r5 = (v) =&gt; Math.round(v * 100000) / 100000;   // match signature precision
        const poly = [];
        const n = Math.min(sig.magnitude.length, sig.angle.length);
        for (let i = 0; i &lt; n; i++) {
          const R = sig.magnitude[i], t = sig.angle[i];
          if (R &gt; 1e-4) poly.push({ x: r5(R * Math.cos(t)), y: r5(R * Math.sin(t)) });
        }
        if (poly.length &gt;= 3) report.featureSet[0].localization_include = [poly];
      }
    }
    if (exclPolys.length) report.featureSet[0].localization_exclude = exclPolys;
```
No coordinate transform — a straight `{x,y}` copy, object-frame mm.

**Load side (round-trip)** — `C:\Users\w2110\Documents\workspace\visSele\UI\WebUI\src\UTIL\InspectionEditorLogic.js:743-765`, `SetDefInfo`:
```js
    const addRegionShapes = (arr, type, baseName) =&gt; {
      if (!Array.isArray(arr)) return;
      arr.forEach((poly, idx) =&gt; {
        if (!Array.isArray(poly) || poly.length &lt; 3) return;
        this.shapeCount = (this.shapeCount || 0) + 1;
        this.shapeList.push({
          id: this.shapeCount,
          type,
          name: baseName + (idx &gt; 0 ? ('_' + idx) : ''),
          points: poly.map((p) =&gt; ({ x: p.x, y: p.y })),
        });
      });
    };
    // addRegionShapes(defInfo.localization_include, 'loc_include', '@__LOC_INCLUDE__');
    // addRegionShapes(defInfo.localization_exclude, 'loc_exclude', '@__LOC_EXCLUDE__');
```
**Important gotcha**: both call sites are **commented out** (`:764-765`) — the load-side rebuild is currently dead, so a saved def's polygons do NOT come back as editable shapes. If your fence needs round-trip editing you must not copy this as-is.

Sample serialized data: `C:\Users\w2110\Documents\workspace\visSele\UI\WebUI\tools\webctl\fixtures\test1.hydef:1257`.

## 3. PARSING (core)

**Member declarations** — `C:\Users\w2110\Documents\workspace\visSele\InspectionCore\MatchingEngine\include\FeatureManager_sig360_circle_line.h:357-364`:
```cpp
  // Pure-SBM native feature-extraction region (object-frame mm, origin-relative). The
  // train-time mask = union(loc_incl_mm) AND-NOT union(loc_excl_mm). This is the
  // SBM-native replacement for the sig360-signature silhouette: a migrated def bakes
  // the signature into loc_incl_mm, a fresh def authors it by hand. Each entry is one
  // polygon. Empty include =&gt; fall back to loc_roi_mm, then the sig360 signature, then
  // Otsu (see trainShapeMatcher mask-priority).
  vector&lt;vector&lt;acv_XY&gt;&gt; loc_incl_mm;   // include polygons (where to extract features)
  vector&lt;vector&lt;acv_XY&gt;&gt; loc_excl_mm;   // exclude polygons ("avoid generation" areas)
```
(Related: `loc_roi_mm` at `:356`, `def_mmpp` at `:333`, `reg_*` at `:347-350`, `m_objLabel` at `:271`.)

**Parser** — `C:\Users\w2110\Documents\workspace\visSele\InspectionCore\MatchingEngine\FeatureManager_sig360_circle_line.cpp:2195-2214`:
```cpp
    auto parse_poly_array = [](cJSON *arr, vector&lt;vector&lt;acv_XY&gt;&gt; &amp;out) {
      out.clear();
      if (arr == NULL || !cJSON_IsArray(arr)) return;
      cJSON *poly = NULL;
      cJSON_ArrayForEach(poly, arr)
      {
        if (!cJSON_IsArray(poly)) continue;
        vector&lt;acv_XY&gt; pts;
        cJSON *pt = NULL;
        cJSON_ArrayForEach(pt, poly)
        {
          cJSON *jx = cJSON_GetObjectItem(pt, "x"), *jy = cJSON_GetObjectItem(pt, "y");
          if (cJSON_IsNumber(jx) &amp;&amp; cJSON_IsNumber(jy))
            pts.push_back(acv_XY((float)jx-&gt;valuedouble, (float)jy-&gt;valuedouble));
        }
        if (pts.size() &gt;= 3) out.push_back(std::move(pts));
      }
    };
    parse_poly_array(cJSON_GetObjectItem(root, "localization_include"), this-&gt;loc_incl_mm);
    parse_poly_array(cJSON_GetObjectItem(root, "localization_exclude"), this-&gt;loc_excl_mm);
```
The lambda is local to `parse_jobj` — to reuse it for a fence key you either add a third `parse_poly_array(...)` line right there (cheapest) or lift the lambda to a file-static.

## 4. USE (core) — building the feature-extraction mask

All in `FeatureManager_sig360_circle_line.cpp`, inside `trainShapeMatcher`.

**The mm→px projector** (`:7106-7118`) — this is the transform you will reuse:
```cpp
  auto render_poly_px = [&amp;](const vector&lt;acv_XY&gt; &amp;poly_mm) {
    std::vector&lt;cv::Point&gt; px;
    px.reserve(poly_mm.size());
    for (const acv_XY &amp;q : poly_mm)
    {
      acv_XY p = TemplateDomain_TO_PixDomain(q, reg_sin, reg_cos, reg_flip_f,
                                             acv_XY(originPx.x, originPx.y), def_mmpp);
      px.push_back(cv::Point((int)lround(p.x), (int)lround(p.y)));
    }
    return px;
  };
```
Note the frame: `originPx` + registered pose (`reg_sin/reg_cos/reg_flip_f`) + `def_mmpp`, i.e. **template/reference-image pixels**, not live-scene pixels.

**Include rasterization** (`:7120-7139`):
```cpp
  if (!loc_incl_mm.empty() &amp;&amp; def_mmpp &gt; 0)
  {
    cv::Mat m = cv::Mat::zeros(templ.size(), CV_8U);
    int filled = 0;
    for (const auto &amp;poly : loc_incl_mm)
    {
      std::vector&lt;cv::Point&gt; px = render_poly_px(poly);
      if (px.size() &gt;= 3) { std::vector&lt;std::vector&lt;cv::Point&gt;&gt; ps{px}; cv::fillPoly(m, ps, cv::Scalar(255)); filled++; }
    }
    if (filled &gt; 0)
    {
      cv::dilate(m, m, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(11, 11)));
      mask = m;
      mask_src = "include";
    }
  }
```
`cv::Mat mask; std::string mask_src = "none";` declared at `:7103-7104`.

**Mask priority chain**: include (`:7124`) → sig360 signature (`:7145-7172`) → Otsu blob (`:7176-7183`) → AND with `loc_roi_mm` (`:7223-7234`) → subtract exclude.

**Exclude subtraction** (`:7238-7247`):
```cpp
  if (!loc_excl_mm.empty() &amp;&amp; !mask.empty() &amp;&amp; def_mmpp &gt; 0)
  {
    int subtracted = 0;
    for (const auto &amp;poly : loc_excl_mm)
    {
      std::vector&lt;cv::Point&gt; px = render_poly_px(poly);
      if (px.size() &gt;= 3) { std::vector&lt;std::vector&lt;cv::Point&gt;&gt; ps{px}; cv::fillPoly(mask, ps, cv::Scalar(0)); subtracted++; }
    }
    if (subtracted &gt; 0) mask_src += "-exclude";
  }
```
Debug dumps: `SHAPE_DRAW_SIG` (`:7186-7217`), `SHAPE_DUMP_MASK` (`:7248-7269`, overlays mask in red plus object-frame axis markers) — worth cloning for the fence.

## 5. THE CONSUMER — `search_point_cv`

**Signature** — `C:\Users\w2110\Documents\workspace\visSele\InspectionCore\MatchingEngine\include\SearchPointCV.h:38-44`:
```cpp
bool search_point_cv(const cv::Mat &amp;gray, acv_XY pt, acv_XY searchDir,
                     float margin, float width, SPEdgeType polarity,
                     float edgeSuppress, float considerRange,
                     float alphaKeep, FeatureManager_BacPac *bacpac,
                     const cv::Mat &amp;labelImg, int objLabel, int maskDilate,
                     acv_XY *outPt, float *outW, int spId = -1,
                     std::vector&lt;CaliperHit&gt; *outHits = nullptr);
```
Contract doc `SearchPointCV.h:28-33`:
```
// labelImg/objLabel (optional): the labeled image + this object's label. When
// provided, a DILATED object mask (label==objLabel, grown by maskDilate px) zeroes
// the sobel response in background BEFORE the local-max search, so the scan can't
// lock onto background specks/dust ... labelImg
// must share `gray`'s coordinate frame (same crop/offset). Pass null to skip.
```
**Frame requirement is load-bearing for you**: `labelImg` must be in the frame of `gray`, which at the call site is `eT.getImageCv()` with the point offset by `off` — i.e. the edgeTracking crop of the *live scene*, NOT the reference-template frame that stage 4's mask lives in.

**Every line inside `SearchPointCV.cpp` that reads `labelImg`/`objLabel`/`mask`:**

- `:18-23` the pixel predicate (label-specific, would be replaced by a plain `mask.at&lt;uchar&gt;`):
```cpp
static inline bool isObjectPx(const cv::Mat &amp;L, int x, int y)
{
  if (L.empty() || x &lt; 0 || y &lt; 0 || x &gt;= L.cols || y &gt;= L.rows) return false;
  const uint8_t *p = L.ptr&lt;uint8_t&gt;(y) + x * 3;
  return !(p[0] == 255 &amp;&amp; p[1] == 255 &amp;&amp; p[2] == 255); // non-white =&gt; object
}
```
- `:24-29` `labelAt()` — **dead code**: defined, never called anywhere in the repo (only hit is its own definition).
- `:85` the gate: `const bool useMask = (!labelImg.empty() &amp;&amp; objLabel &gt;= 0);`
- `:89-90` allocation: `cv::Mat mask; ... if (useMask) mask = cv::Mat(nS, nP, CV_8U, cv::Scalar(255));`
- `:95` per-row pointer: `unsigned char *m = useMask ? mask.ptr&lt;unsigned char&gt;(i) : nullptr;`
- `:99` off-image sample zeroes the mask cell too: `... { d[j] = 0; vv[j] = 0; if (m) m[j] = 0; continue; }`
- `:106` the only `labelImg` read: `if (m) m[j] = isObjectPx(labelImg, (int)(q.x + 0.5f), (int)(q.y + 0.5f)) ? 255 : 0;`
- `:110-121` the ring (silhouette-only) transform, gated on `useMask &amp;&amp; maskDilate &gt; 0`:
```cpp
  if (useMask &amp;&amp; maskDilate &gt; 0)
  {
    int k = 2 * maskDilate + 1;
    cv::Mat se = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(k, k)), md, me;
    cv::dilate(mask, md, se); cv::erode(mask, me, se);
    for (int i = 0; i &lt; nS; i++) {
      unsigned char *pd = md.ptr&lt;unsigned char&gt;(i), *pe = me.ptr&lt;unsigned char&gt;(i), *m = mask.ptr&lt;unsigned char&gt;(i);
      for (int j = 0; j &lt; nP; j++) m[j] = (pd[j] &amp;&amp; !pe[j]) ? 255 : 0;
    }
  }
```
For a *measurement fence* you almost certainly want the fill, not the ring — so `maskDilate=0` (ring off) is the right default.
- `:163` `const unsigned char *m = useMask ? mask.ptr&lt;unsigned char&gt;(i) : nullptr;`
- `:173` **the actual suppression**, one line, applied before local-max detection: `if (useMask &amp;&amp; !m[j]) e = 0;`
- `:236` / `:244` debug dump under env `SPCV_DUMP` → `/tmp/spcvmask_sp%d_pt%d_%d.png`.

The consumer only ever needs a binary CV_8U; sampling is nearest-neighbour at `(q.x+0.5, q.y+0.5)` per band cell — so a fence mask can be a full-scene CV_8U with 255 = "measurement allowed".

**Call sites (only two in the repo):**

1. Production — `FeatureManager_sig360_circle_line.cpp:1275-1279`:
```cpp
      ok = search_point_cv(eT.getImageCv(), acvVecSub(pt, off), barVec,
                           margin, width, sp_et, edgeSuppress,
                           includeRangePx, alphaKeep,
                           eT.getBacpac(), labelImg, m_objLabel, maskDilate,
                           &amp;out, &amp;str, def.id, &amp;rep.cal_hits);
```
with `cv::Mat labelImg;` — a **deliberately default-constructed empty Mat** — at `:1237`, and `maskDilate` computed at `:1271-1272`:
```cpp
      int   maskDilate = (said &amp; featureDef_searchPoint::EDGE_SET_MASK_DILATE)
                         ? def.mask_dilate : 0;
```

2. Test — `InspectionCore/test_suite/test_searchpoint_frame_edge.cpp:65-67`: `..., nullptr, cv::Mat(), -1, 0, &amp;out, &amp;w, -1, &amp;hits);`

**Dead/commented references to the disabled mask** (commit `874b9b08` = `wip(M2): search_point_cv cap-finder (SobelY + per-column local-maxima + min-Y top)`):

`FeatureManager_sig360_circle_line.cpp:1202-1236` is a 35-line comment block — no commented-out code line survives (the original `m_labeledImg_cv` expression was deleted, not commented), and `grep m_labeledImg` over all `.cpp`/`.h` returns only lines `1207` and `1218` of this comment. Key excerpts:
```cpp
      // THE BACKGROUND MASK IS OFF, AND HAS BEEN SINCE 2026-05-29.
      ...
      // That makes mask_dilate inert too: useMask is (!labelImg.empty() &amp;&amp;
      // objLabel &gt;= 0), and this is always empty. The knob is honoured all the
      // way to search_point_cv and then has nothing to act on.
      ...
      // And do NOT wire localization_include/localization_exclude in here
      // either, however close they look. Those polygons are the FEATURE
      // GENERATION mask: they say where line2Dup extracts features in order to
      // FIND the part. This is a MEASUREMENT mask: it says where a caliper scan
      // may pick up an edge once the part has been found. Two different jobs,
      ...
      // The successor is a separate object polygon mask, planned but NOT yet
      // specified anywhere. The consumer inside search_point_cv is reusable
      // as-is when it arrives -- it only needs a binary mask -- but the
      // labelImg/objLabel pair should become one, since isObjectPx is a
      // label-specific test. See BACKLOG_2026-08-26.
      cv::Mat labelImg;
```
`m_objLabel` itself: declared `FeatureManager_sig360_circle_line.h:269-271`, assigned only at `FeatureManager_sig360_circle_line.cpp:6544` (`m_objLabel = i;`), and read only by the call above — so it is effectively vestigial too.

## Design notes for the parallel "measurement fence"

- The backlog already specs this: `InspectionCore/docs/BACKLOG_2026-08-26.md:105-166` (esp. the stage table at `:145-148` and the "consumer should become a plain CV_8U mask" note at `:150-166`). Also `InspectionCore/docs/pure_sbm_def_design.md:132-134,153,203-208`, `InspectionCore/docs/search_point_rework.md:113,137,337`, `UI/WebUI/docs/WEBUI_CAVEATS.md:25-33`.
- Reusable verbatim: `_locRegionCommon.jsx` (pass a new stroke + label), `parse_poly_array` (one more call line), `render_poly_px`'s `TemplateDomain_TO_PixDomain` idiom, and the `useMask`/`e = 0` consumer.
- The one real gap: stage 4's mask is rasterized in **template/reference-image** pixels at train time, while `search_point_cv` wants a mask in the **live edgeTracking crop** frame at inspect time. Your fence must be rendered per-inspection using the matched pose + `off`, not reused from `trainShapeMatcher`.
- Two `MISC_Util.js` traps to mirror: the strip from `features[]` must be unconditional (`:349-356`, not inside the `shape_based` branch), and unlike `localization_include` a measurement fence should probably be emitted regardless of `locating_engine`.
- The load-side `addRegionShapes` calls are commented out (`InspectionEditorLogic.js:764-765`), so today no region round-trips into the editor.

---

## Audit the ESP32 firmware

`task-id aabf2bb0e3d97c19a`  9677 chars

Audit complete. Findings ranked most-severe first.

---

**1. CONFIRMED — `dbg_printf`/`msg_printf` overflow `dbgBuff` when the format output is truncated**
`src/app/LegacyFirmware.cpp:8132-8140` (and identically `8163-8171`), buffer at `:3499` (`char dbgBuff[500]`, a member of `MData_JR`).

```c
ret = vsnprintf (str, restL-10, fmt, aptr);
str+=ret;                       // &lt;-- ret is the WOULD-HAVE-WRITTEN length
...
int len=sprintf(str,"\"}");     // writes past the end of dbgBuff
return send_json_string(0,(uint8_t*)dbgBuff,str-dbgBuff,0);
```
`vsnprintf` returns the length it *would* have produced, not the truncated length. On truncation `str` lands past the buffer, `sprintf` writes `"\}` out of bounds, and `send_json_string` is then handed `len &gt; sizeof(dbgBuff)` — it transmits 500+ bytes of adjacent object memory and CRCs it.

Reachable path: `MData_JR::recv_ERROR` (`:4372`) calls `dbg_printf("recv_ERROR:%d %s dat:%s", errorcode, dataBuff, hex)` where `dataBuff` is `uint8_t[2048]` (`include/comm/Data_Layer_Protocol.hpp:26`). A `RECV_BUFFER_FULL` latch is raised with `buffIdx == 2048`, so the `%s` alone is 2048 bytes into a 500-byte buffer: `str` ends up ~1600 bytes past `dbgBuff`, and `dbgBuff` is followed by other `MData_JR` members. This is a remote (host-link) memory corruptor in the protocol *error* handler — i.e. it fires exactly when the link is already misbehaving.

**2. CONFIRMED — 1-byte OOB write in `recv_ERROR` on `RECV_BUFFER_FULL`**
`src/app/LegacyFirmware.cpp:4348`: `dataBuff[buffIdx]='\0';`
`src/comm/Data_Layer_Protocol.cpp:371` raises `RECV_BUFFER_FULL` when `buffIdx==sizeof(dataBuff)` (2048), and `enterProtocolError` (`:34-45`) calls `recv_ERROR` directly. `dataBuff[2048]` is one past the array; the next member in declaration order is `int buffIdx` (`Data_Layer_Protocol.hpp:26-27`), so the NUL lands in `buffIdx`'s low byte on a little-endian target. The same loop at `:4342` (`for i&lt;buffIdx`) is in bounds, so only the terminator is OOB. Precedes and compounds defect 1.

**3. CONFIRMED — SEL ON can be queued without its OFF: a latched air valve**
`src/app/LegacyFirmware.cpp:3219-3220`, `3224-3225`, `3244-3245`.
`ActRegister_pipeLineInfo` (`:2967`) carefully pre-checks `space() &gt;= 2` for L1A/CAM1/L2A/CAM2 and `&gt;= 1` for SWITCH — but the SEL queues are never space-checked anywhere. In the SWITCH branch each verdict does two unguarded `ACT_PUSH_TASK`s, and `ACT_PUSH_TASK` silently does nothing when `getHead()` returns NULL (`:1821-1834`, `RingBuf.hpp:246-250`).

Failure scenario: `ACT_SEL{1,2,3}` reaches exactly `space()==1`. The `*_on` push succeeds, the `*_off` push is dropped. `Run_ACTS` fires `IO_ON(PIN_O_SELn)` and nothing will ever drive it off. `selSafeService` (`:2275`) only helps when `SEL_SAFE_AT_MS` was armed (state transition / queue clear, `:2533/:2739/:3485`), never during a normal run. The valve stays energised until the next stop. `ACT_SEL3` is the OK outlet and takes a push for *every* good part, so it is the one most likely to fill.

**4. CONFIRMED — host-link watchdog cannot stop the machine in `INSPECTION_MODE_TEST`**
`src/app/LegacyFirmware.cpp:8624-8637`. `hostNeeded` includes `SYS_STATE::INSPECTION_MODE_TEST`, and the watchdog answers with `SYS_STATE_Transfer(INSPECTION_ERROR, HOST_LINK_TIMEOUT)`. But the transition table (`include/core/FirmwareTypes.hpp:84-86`) gives `INSPECTION_MODE_TEST` exactly one arc — `EXIT_INSPECTION_MODE` — with no `INSPECTION_ERROR`. The transfer is a no-op, the condition (`last_rx_ms` frozen) stays true, and the loop re-issues it every iteration forever: no stop, no `ALL_OUTPUTS_SAFE`, and no counter save (which the comment says "rides on the way into ERROR"). The plate keeps turning past an unmanned selector — the exact failure the watchdog was extended to cover. The other four `hostNeeded` states all have the arc.

**5. CONFIRMED — `MachineConfig::clear()` reports success on a partial clear**
`src/config/MachineConfig.cpp:437-443`.
```c
bool ok = false;
if (prefs.isKey(kJsonKey)) ok |= prefs.remove(kJsonKey);
if (prefs.isKey(kBlobKey)) ok |= prefs.remove(kBlobKey);
```
`|=` means one success masks the other's failure. If the JSON is removed but the legacy blob's `remove` fails, `clear_saved_setup` acks `cleared:true` and the next boot loads the superseded packed blob — reinstating `io_inv_mask` and every offset the operator asked the machine to forget. This is the precise failure the surrounding comment says the two-store clear exists to prevent. Separately, when *neither* key exists `ok` stays `false`, so a clean board reports `cleared:false`.

**6. CONFIRMED — legacy-blob boots report `defaultedCount()==0`, i.e. "nothing is on a compiled default"**
`src/config/MachineConfig.cpp:346` vs `:357-399`. `computeDefaulted()` is called only on the JSON branch. The legacy packed-blob branch returns at `:400` with `defaulted[]` still all-zero and `defaultedN==0`. A v≤9 board is precisely the one where most of `cfgKeyAt()`'s ~68 keys are *not* in the struct and are silently running compiled defaults — and it is the board that answers "0 defaulted". The header comment justifies the mask over a name list on the grounds that "a migration UI that shows a subset is how somebody migrates half a config and believes they are finished"; this path shows the empty set. Stale-key reporting is likewise skipped for the blob path.

**7. SUSPECTED — `staleVals` uses substring matching, so a *known* key's value can be reported as stale**
`src/config/MachineConfig.cpp:292`: `if (!strstr(staleKeys, dotted)) return;`
`staleKeys` is a comma-joined list; membership is tested by substring. Any key whose dotted name is a substring of an entry gets its value emitted. E.g. a stale `cam.cal_pulse_us` puts that text in `staleKeys`, and a top-level key literally named `cam` (or a group key `cal_pulse` under any group) then also passes the test. `cfg_stale_values` then lists more entries than `cfg_stale_n`, which is the count/list disagreement `CFG_STALE_TRUNC` was added to make visible — and here it disagrees in the *other* direction, unflagged.

**8. SUSPECTED — a stored `machine_id` is not restored on the JSON path, contradicting the documented contract**
`src/config/MachineConfig.cpp:571-582` states "A stored id still wins if one is present, so boards commissioned under the old scheme keep the name their records were filed under." `MachineConfig::setMachineId` has **no caller** (grep across `src/app` and `src/config` finds only the definition), and `setMachineSetup` deliberately ignores `machine_id` (`LegacyFirmware.cpp:9559`). Only `applyToGlobals` — the legacy *packed-blob* path — restores it. So the moment a board saves once as JSON, the stored id becomes unreadable and `machineId()` silently falls back to the eFuse-derived `uI-XXXXXXXXXXXX`. Same chip so the value is stable, but a board commissioned with a *custom* id loses it, and inspection records change identity across one save — the exact silent mis-attribution the comment block is about.

**9. SUSPECTED — `send_data` ignores the byte count, so a short write ships a truncated frame with a valid-looking trailer**
`src/app/LegacyFirmware.cpp:8112-8115` (`Serial.write(data,len)`, return discarded) called from `Data_JsonRaw_Layer::send_json_string` (`src/comm/Data_Layer_Protocol.cpp:211-217`). The CRC trailer is computed over the *intended* `len` and appended unconditionally in a second `send_data`. If the UART TX path ever returns short (small TX buffer, `setTxBufferSize` configured non-blocking), the host receives a truncated JSON body followed by a CRC of bytes it never got — a guaranteed frame drop with no device-side counter. This is the "sent truncated" case the retdoc overflow guards at `:8081-8109` were built to eliminate on the other side of the wall; the transmit primitive has no equivalent check.

**10. SUSPECTED — `gateInfo.debounce` is `uint16_t`, `DEBOUNCE_*_THRES` is `int`, clamped only on the low side**
`src/app/LegacyFirmware.cpp:3647` (`uint16_t debounce`) vs `:9865-9866` (`if(DEBOUNCE_H_THRES&lt;1)…=1;`, no upper bound). Setting `gate.debounce_rise` to 65536 (or any multiple) truncates to 0 on load into the counter; `gateInfo.debounce--` at `:3745`/`:3757` then wraps to 65535 and the edge needs 65535 consecutive samples to confirm — the gate goes deaf. `get_setup` cheerfully reports `debounce_rise: 65536` (`:9177`), so the reported config and the running config disagree with nothing to say so.

**11. SUSPECTED (latent, build-fragile) — trailing whitespace after the line-continuation backslash**
`src/app/LegacyFirmware.cpp:3416` — `#define ACT_TRY_RUN_TASK(act_rb, cur_pulse, live_off, cmd_task) \ ` has a space between `\` and the newline. GCC splices it with a warning, so it builds today; a stricter preprocessor truncates the macro to its first line and every stage stops firing. Worth removing given the macro drives all eight actuation queues.

---

**Checked and found sound** (so they are not re-investigated): the unsigned wrap arithmetic in `ACT_TRY_RUN_TASK` (`(cur_pulse - task-&gt;gate_pulse) &gt;= task_off` and the `_cap_` relative subtraction) is correct across the 2^32 rollover; `RingBuf::getTail(idx)` is properly bounds-checked so the `getTail(1)` cap lookup cannot read a stale slot; `trustedPrefix()` clamping via `if (want &gt; head) want = head` correctly handles short blobs, and `kMinCompatBytes` sits past `io_inv_mask` so the `raw.version &gt;= 4` arm decision cannot be made on bytes that were never read; `retdoc` and `doc` are cleared per command (`:5161-5162`) and the `overflowed()`/`slen` guards at `:8081-8109` do cover the get_setup and set_setup replies.

---

## Analyse primitive features in DefConfUI

`task-id a6147d2acc816082a`  9382 chars

I read the UI shape modules, property sheets, the DefConf overlay path, and the core caliper/search-point code end to end. Findings, most severe first.

## CONFIRMED

**1. `edge.polarity: 'any'` on a search_point is silently executed as `falling`.**
`InspectionCore/MatchingEngine/FeatureManager_sig360_circle_line.cpp:1162-1165`
```cpp
SPEdgeType sp_et = SP_LIGHT_TO_DARK;
if (def.edge_polarity == RISING) sp_et = SP_DARK_TO_LIGHT;
else if (def.edge_polarity == FALLING) sp_et = SP_LIGHT_TO_DARK;
```
`ANY` falls through to the initializer. `SearchPointCV.cpp` *has* a bidirectional mode (`SP_BOTH`, `e = fabsf(e)` in the `sgn` lambda) and it is unreachable from the def. Meanwhile `'any'` is the **default** the UI seeds on every new search_point (`EverCheckCanvasComponent.js:2645`, `SearchPointPropertySheet.jsx:276`), and the dropdown (`search_point.js:358`, sheet line 327) offers all three.
Scenario: part is bright-on-dark (rising edge). Operator leaves polarity at the default `any`, expecting "either direction". The core only accepts light→dark, finds nothing on the real edge, locks onto whatever weaker light→dark gradient exists in the band or returns NA. Nothing on screen says the selected polarity was not honoured.

**2. On a failed fit, `cal_hits` are drawn in the wrong coordinate frame.**
`UI/WebUI/src/UTIL/InspectionEditorLogic.js:1127-1130` (NA early-return) assigns `eObject.cal_hits = _naHits` **raw**, while all three success paths (lines 1208, 1239, 1279) go through `cal_hits_forward()`. Hits arrive in object-frame mm; InspectionUI (`oriBase=false`) renders image-frame.
Scenario: a caliper line goes NA in inspection. Its per-caliper hits — the exact overlay you look at to find out *why* it failed — are rendered un-rotated and un-translated, so they land near the canvas origin instead of on the part. The success case is correct, so the bug only appears when you most need the overlay. (Also note the NA branch returns *before* the `pt1/pt2` forward transform, so nothing else on that shape moves either — the hits are the only thing that looks placed.)

**3. The DefConf stale-hits gate misses the fields that define a search_point's search region.**
Fingerprint built at `UI/WebUI/src/redux/reducer/UICtrlReducer.js:220-224` and compared at `EverCheckCanvasComponent.js:2240-2244` — both hash only `pt1/pt2/pt3, margin, locating, caliper, edge`.
`width`, `angleDeg`, `search_far` and `ref` are omitted, yet all four change the search band (`width` → `nS` rows in `search_point_cv`, `angleDeg`/`ref` → `barVec`, `search_far` → scan side). Same for arc `direction`/`fit_mode`.
Scenario: run an inspection, then rotate the search point 90° or double its width. The gate's whole purpose is to hide hits that no longer reflect the current params; the fingerprint still matches, so the old hits stay on screen pinned to the *new* box, reading as a fresh confirmation of a config that was never run.

**4. Arc caliper auto-width is computed from the complementary arc.**
`shapes/arc.js:186-191` (onChange), `arc.js:216-220` (`geomLengthFn`), `_propertySheet/ArcPropertySheet.jsx:138-142` — all three compute `span = atan2(pt3) - atan2(pt1)` normalized to `[0,2π)`, ignoring pt2. The *drawing* code 30 lines below (`arc.js:299-304`) correctly ports the core's `convert3Pts2ArcData` through-pt2 selection, and picks `a0=pt3, a1=pt1` when `angle31 &lt;= angle21`.
Scenario: draw an arc whose 3 points make the CCW sweep run pt3→pt1 (i.e. clicked "the other way"). Real span is `2π − s`; the seeder uses `s`. Flip to caliper: a 30° arc gets width seeded from 330° of arc, so 10 boxes of ~11× the intended width — they overlap massively, each caliper averages across the whole feature, and the fit is garbage. Nothing warns; the boxes are drawn at the right *places*, just the wrong size.

**5. UI and core disagree on degenerate caliper `count`/`width`.**
`shapes/_caliperFields.js:146-147` and `172-173`:
```js
const count = (caliper.count &gt; 0) ? Math.min(caliper.count, 512) : 10;
const width = (caliper.width &gt; 0) ? Math.min(caliper.width, 64)  : 0.1;
```
vs core `Caliper.cpp:257` (`if (count &lt; 2) count = 2;`), `Caliper.cpp:552` (`if (count &lt; 3) count = 3;`), and the parse defaults `cal_width = 0.5` (`FeatureManager_sig360_circle_line.cpp:260, 1622`).
Three concrete divergences: (a) `count = 0` → UI draws **10** boxes, core runs **2** (line) / **3** (arc); (b) `count = 1` → UI draws one box at the *midpoint* (`lineCaliperAnchors` line 84), core runs two at the *endpoints*; (c) a shape whose `locating` is `caliper` but has no `caliper` object (a def authored elsewhere, or an override row that sets only `locating`) → UI draws 0.1 mm-wide boxes, core uses 0.5 mm — 5× the projection width, silently.

**6. `min_inliers &gt; count` is accepted and produces permanent, unexplained NA.**
`LinePropertySheet.jsx:91` / `ArcPropertySheet.jsx:208` write `min_inliers` with no relation to `count` (default seed is `count:10, min_inliers:5`). Core: `Caliper.cpp` line-path `r.ok = fitOk &amp;&amp; (ni &gt;= minInliers)` with `minInliers = cal.min_inliers`.
Scenario: operator reduces `count` to 4 for speed and leaves `min_inliers` at 5. `ni ≤ 4 &lt; 5` always, so `ok` is false for every part forever, while the overlay shows 4 perfectly green inlier crosses on the edge. The one screen that could explain the NA actively argues against it.

**7. `lockCaliper` (shape_based defs) is never applied to search_point.**
`DefConfUI.js:2316` computes `lockCaliper` and passes it at line 2379 to every PropertySheet; `LinePropertySheet.jsx:62-66` and `ArcPropertySheet.jsx:176-180` force `locating='caliper'` and hide the selector. `SearchPointPropertySheet.jsx:258` does not accept the prop at all — the search_point keeps its `contour` default and still shows the dropdown.
Per DefConfUI's own comment ("the raw-gray path has no contour to follow"), a contour search_point in a shape_based def has nothing to search. CONFIRMED as an inconsistency in the code; whether search_point was *meant* to be included is SUSPECTED.

**8. `edge.min_strength = 0` is displayed but not used, for search_point only.**
`FeatureManager_sig360_circle_line.cpp:1174`: `edgeSuppress = (def.edge_min_strength &gt; 0) ? def.edge_min_strength : 10.0f`. Same for `blur` (→3), `mask_dilate` (→8), `include_range` (→2 px). The property sheet shows `0` in an editable field and the core runs `10`. Legacy defs (`test1.hydef` has `include_range: 0`) all sit on these hidden substitutions. Low severity but it is exactly the "a value that is wrong with nothing on screen saying so" class.

## SUSPECTED

**9. Arc caliper-box status colors misalign on a flipped part.**
`strokeForAnchor(calHits, i)` (`_caliperFields.js:119-122`) is index-aligned with `arcCaliperAnchors`. The core recomputes `convert3Pts2ArcData` *after* the pose+flip transform (`FeatureManager_sig360_circle_line.cpp:4242`), so for a mirrored part the CCW-through-pt2 selection picks the opposite endpoint as `sAngle` — core caliper index 0 is the UI's index `count-1`. The X marks are fine (they carry coordinates), but the *grayed-out box* marking "this caliper found nothing" appears at the wrong end of the arc. I did not trace a concrete flipped run to confirm the ordering reverses in every case.

**10. search_point scan side reverses for flipped parts.**
`FeatureManager_sig360_circle_line.cpp:1099-1102` negates `vec` when `flip_f &gt; 0` (i.e. **not** flipped) — the asymmetric branch. Working the signs through, the UI's `cnormal` (`search_point.js:412-413`) matches the core's `perp` for unflipped parts and is 180° off for flipped ones, so the "first hit along the ray" is taken from the opposite side of the band for a mirrored part. The line path handles flip deliberately (`LineMatching_caliper` swaps p0/p1 with a comment explaining polarity/scan-order); this one looks like the same problem solved on the wrong branch. Needs a mirrored-part run to confirm.

## Actively misleading comments

- `shapes/line.js:17` "core default: count=30" and `shapes/arc.js:166` "core default for arc: count=36" — the core default is **10** for both (`FeatureManager_sig360_circle_line.cpp:260, 1622`), and the UI seeds 10.
- `UTIL/InspectionEditorLogic.js:1216-1217`: "When the whole fit FAILED (status != SUCCESS), force every hit to status=0 so the WebUI grays all boxes." The very next line says the opposite and the code does the opposite. Two contradictory instructions in one comment block, directly above the code that decides what a failed caliper looks like.
- `shapes/_caliperFields.js:7`: documents the wire shape as `caliper = { count, width, length, step }`. The UI actually writes `{count, width, min_inliers, max_error}` and never writes `length`/`step`. (Lines 25-26 below it get it right, which makes the header worse, not better.)
- `shapes/search_point.js:343`: "the core runs a single `caliper_measure` along the search vector". It runs `search_point_cv` (`FeatureManager_sig360_circle_line.cpp:1179`) — a different algorithm with different polarity handling (finding #1) and different parameter units. `caliper_measure` is the arc path.
- `_caliperFields.js:340-359` `drawEdgePolarityArrow` is dead code — no caller anywhere in `UI/WebUI/src`. Its "RISING points into light" convention is the documentation someone would reach for when checking finding #1, and it is not what runs.

---

## Audit launcher and update path

`task-id ae278c24439fc726a`  9265 chars

Audit complete. Findings ranked, most severe first.

## 1. `pickAppRoot` accepts any folder; nothing enforces `workingDir` ⊄ `appRoot` — CONFIRMED
`UI/Launcher/main.js:376-385` (`cfg.set('appRoot', chosen)` with no validation) + `UI/Launcher/src/apps.js:413-425` (`prune`) + `UI/Launcher/main.js:255`.
config.js:374-383 states the working dir is "deliberately OUTSIDE appRoot" — that is a comment, not a check. An operator who points the app folder at the machine's data drive (or at the working dir, or any parent of it) gets no warning. The next successful start calls `apps.prune()`, which deletes every subdirectory under appRoot that is not the current version — `data/`, calibration, recipes, snapshots. Silent, irreversible, and it happens on the *success* path. The same hole covers the known launcher-folder case; it needs one guard at the point of choosing, comparing the resolved paths in both directions.

## 2. A spawn failure leaves the launcher permanently believing the core is running — CONFIRMED
`UI/Launcher/src/supervisor.js:194-200`. `entry.exited` is set only in the `'exit'` handler (:209). Node does not emit `'exit'` when the process could not be spawned — only `'error'` (:199), which just logs a line. So on EACCES / EBUSY / antivirus block / exe-is-a-directory:
- `running` (:68-71) stays true forever, `pid` is `undefined`;
- `_onChildExit` never runs, so no `'exit'` event → `main.js:269` never shows the crash screen and the operator is never told why;
- `waitUntilReady` burns the full `readyTimeoutMs` (40 s for this payload), then `startCore` prunes and shows the app UI over a dead core (`main.js:251-265`);
- every later `startCore` throws "already running" (:160), `stop()` waits the full shutdown timeout and force-kills `pid` `undefined`, and `before-quit` (`main.js:438`) blocks the same way on every close.

The fix is to set `entry.exited = true` in the `'error'` handler (or bind on `'close'`) and route it through `_onChildExit`.

## 3. `showApp` sets `uiState='app'` before loading, and does not handle a load failure — CONFIRMED
`UI/Launcher/main.js:124-129`, unguarded, awaited at :261 with no try/catch; `startCore` is itself awaited uncaught at :430.
If `loadURL`/`loadFile` rejects (UI served by the core and its port is not up yet, missing index, a bad `INSP_UI_DEV_URL`), the window keeps showing the launcher shell but `uiState` is already `'app'`. Every `assertShell` action (:287) then refuses with "…is only available from the launcher screen" while the launcher screen is exactly what is on screen: install, select version and start are all dead, with no route back except killing the process. Same path at startup gives an unhandled rejection and no message at all. `uiState` should flip only after the load resolves, and the failure should land on `showShell({kind:...})`.

## 4. Replacing an installed version deletes it before the new one is in place — CONFIRMED
`UI/Launcher/src/updater.js:218-228`. `fs.rmSync(dest)` then `fs.renameSync(root, dest)`. If the rename fails (a locked file left in `dest` so `rmSync` partially succeeded, EXDEV if `appRoot` and the staging parent ever diverge, an antivirus holding a handle), the old copy of that version is gone, the new one is not installed, and the `finally` at :236 deletes the staged tree too. Net result of a failed install: one fewer working version to roll back to — the opposite of what the file header promises. Rename the old `dest` aside, rename the new one in, then delete.

## 5. `prune` protects the name in `current.json`, not the version that is actually running — CONFIRMED (conditional)
`UI/Launcher/src/apps.js:413-416` vs `resolve()` :385-396 and `main.js:255`.
`resolve()` deliberately falls back to the newest *valid* version when the pointer is missing or names something broken, and starts it. `prune` then computes `cur = this.currentVersion()` — null, or a name matching no directory — so the running version is in the doomed list like any other. `list()` includes invalid directories, so if enough invalid or newer-named dirs sort above it, the version currently executing is at index ≥ `keep-1` and its directory is deleted out from under the live process. `prune` must be told which version was started, not re-derive it.

## 6. Config values from `launcher.json` are accepted unvalidated and drive timers directly — CONFIRMED
`UI/Launcher/src/config.js:451-453` (any value for a known key) and `:484-488` (`set` checks only the key name). Consequences:
- `keepVersions: 0` or `"three"` → `apps.js:416` `slice(Math.max(0, NaN))` = `slice(0)` → **every** non-current version deleted, no rollback left.
- `shutdownTimeoutMs: null` → `supervisor.js:327` `setTimeout(…, 0)` → every stop is an immediate force kill, and the log line then claims the core "did not exit within null ms".
- `pingIntervalMs: 0/null` → `supervisor.js:290` `setInterval(…, 0)` → a health-check hot loop hammering the control socket.
Each key needs a type/range clamp at load, falling back to the default with a `loadError`.

## 7. Renderer navigation is not actually restricted, and `setWindowOpenHandler` opens any URL externally — CONFIRMED
`UI/Launcher/main.js:96-99`. The comment says the renderer "must not be able to navigate anywhere we did not send it", but only `setWindowOpenHandler` is installed — there is no `will-navigate` handler. The preload applies to whatever the window navigates to, so an app UI page that navigates off-origin keeps `window.launcher` (`pickFolder`, `stopCore`, and the shell channels whenever `uiState` is `'shell'`). Separately, the handler passes `url` to `shell.openExternal` with no scheme allowlist, so the renderer can have the main process hand arbitrary URIs (`file:`, custom protocol handlers) to the OS.

## 8. `launcher:status` re-executes payload code every 5 seconds — CONFIRMED
`UI/Launcher/shell/shell.js:353` → `main.js:300` → `currentPlan()` :177 → `boot.load()` → `boot.js:120-133` deletes the require cache and re-`require`s the version's `scripts/boot.js`, runs `describe()`, and then runs the `checkRequirements` hook (`main.js:198`). Payload code — which may spawn helpers via `services.run` — executes on every poll, including while the core is running, and every re-require re-evaluates the module (its old copy is unreachable but the churn is real). The plan should be computed once and cached until something invalidates it.

## 9. `requires[].path` is resolved against the launcher's process cwd — CONFIRMED
`UI/Launcher/src/boot.js:238`, `path.resolve(r.path)`. Every other application-supplied path goes through `resolveInApp`; this one does not, and there is no base, so a relative `requires` path resolves against Electron's cwd (the exe directory, or `C:\` when launched from a shortcut). The requirement check then tests a path unrelated to the machine, and either blocks start with a wrong reason or passes a genuinely missing directory. This repo's own payload (`scripts/boot.js:118`) happens to pass an absolute path, which is why it has not been seen. Resolve relative entries against `workingDir`.

## 10. Verification is install-time only; nothing re-checks before executing a version — SUSPECTED
`UI/Launcher/src/updater.js:199-204` hashes the package, but `apps.validate()` (`apps.js:353-362`) checks only that two entries exist, and `boot.load` `require()`s `scripts/boot.js` into the **main** process with full Node privileges. `make_package.py` deliberately removes `manifest.json` from the source tree but it *is* shipped in the package, so it is on disk in the installed version and could be re-verified before `require`. As it stands, any post-install corruption or tampering of an installed version is executed unverified — which undercuts the "verify-then-execute" claim in the boot.js header.

## 11. `stop()` leaks a `'line'` listener on every timeout — CONFIRMED (minor)
`UI/Launcher/src/supervisor.js:311-315`. `check` removes itself only when it observes `!this.running`. On the timeout path (:328-338) and when the force kill does not land within 3 s, the listener stays attached to the Supervisor forever. Repeated stop cycles accumulate listeners on the hot `'line'` path (every log line invokes all of them) and will eventually trip the MaxListeners warning.

## 12. Two smaller packaging/extraction items — SUSPECTED
- `UI/Launcher/tools/make_package.py:~140`, `c.isalnum()` is Unicode-aware, so a version like `版1.0` passes the packaging check and is then rejected by `updater.js:157`'s ASCII regex — the failure moves from the bench to the machine, which is exactly what the file's own header says it exists to prevent.
- `updater.js:100-107` `walk` treats a symlink as a file (the dirent is not a directory) and `sha256File` follows it to its target, so a symlink entry in the zip can be manifest-clean while pointing outside the version directory — `resolveInApp` validates path text, not links. Depends on whether bsdtar/Expand-Archive materialize links in the target environment.

Also worth noting (no code change implied): `make_package.py` writes `manifest.json` into the *source* tree and mutates the caller's `info.json` when `--version` is passed, and only files are zipped, so an empty directory the application needs does not survive packaging.

---

## Audit the camera layer

`task-id af4ed9034bac879aa`  8885 chars

Audit complete. Findings below, most severe first.

## CONFIRMED

**1. Integer divide-by-zero crash on any absent/unreadable int node — `CameraLayer_HikRobot_Camera.hpp:142-150` + `:211`**
`SetIntValue_w_Check()` discards the return of `GetIntValue()`. On failure the `MVCC_INTVALUE_EX intValInfo = {0}` stays all-zero and is passed to `leastSatiValue()`, which at line 211 computes `range.nMin + ((target - range.nMin) / range.nInc) * range.nInc` with `nInc == 0` → int64 division by zero → SIGFPE / EXCEPTION_INT_DIVIDE_BY_ZERO, process dies.
Reachable path: `TriggerMode()` line 937 `SetIntValue_w_Check("AcquisitionBurstFrameCount", 1)` — whose own comment says "harmless if the node is absent". It is not harmless: an absent node crashes the core inside `TriggerMode`, i.e. during construction (`:675`) or on any `trigger_mode` change from `CameraSetup` (`wiringPanel.cpp:2601`). Same exposure for `Width/Height/OffsetX/OffsetY` in `SetROI` (`:80-85`) if the get fails while the device is still streaming. Note the repo carries ~20 crash dumps from 2026-08-19..23.

**2. `SnapFrame` early-returns without restoring the callback — `CameraLayer.cpp:114-120`**
`for (int i = 0; TriggerCount(1) == NAK; i++) { if (i &gt; 5) return NAK; }` returns while `callback == SNAP_Callback` and `context == cb_param` (the *caller's stack* object), and with `snapFlag` left at 1. Every subsequent frame invokes `SNAP_Callback` against a destroyed stack frame — heap/stack corruption, and `_snap_cb` is never cleared. Also skips the `cb_swap_m` restore block entirely.

**3. `frame_lost` unsigned underflow on frame-counter reset — `wiringPanel.cpp:7441-7446`**
`const uint32_t d = fnow - _prevFrameNum; // unsigned: wrap is fine`. Wrap at 2^32 is fine; a *reset* is not. `nFrameNum` restarts at 0 on every `MV_CC_StartGrabbing`, and `StartAcquisition()` is re-run by `SetROI`, `SetMirror`, `TriggerMode` and `CameraSetup:2770`. First frame after any of those gives `d ≈ 4.29e9`, so `g_camFrameLost` (uint32) jumps by ~4.29 billion and `frame_gap_n` by 1, both reported to the WebUI at `:4172-4175`. The statics `_prevFrameNum/_haveFrameNum` (`:7436-7437`) are function-local and never invalidated on camera restart/reconnect.

**4. `TriggerMode` ON status discarded by shadowing — `CameraLayer_HikRobot_Camera.cpp:943` vs `:949`/`:962`**
`int nRet = SetEnumValue("TriggerMode", MV_TRIGGER_MODE_ON);` is shadowed by a second `int nRet` inside each of the `type==1` and `type==2` blocks. The outer value is never read: if the camera refuses to enter trigger mode the function still returns ACK as long as `TriggerSource` took. Result is a free-running camera the core believes is hardware-triggered. `CameraSetup`'s `_chk("trigger_mode", …)` therefore cannot see it.

**5. `SetROI` leaves a half-applied ROI on any rejected write — `CameraLayer_HikRobot_Camera.cpp:80-99`**
`OffsetX`/`OffsetY` are unconditionally zeroed first (`:80-81`), then W/H/X/Y written. If any write is rejected (most likely: `StopAcquisition()` at `:34` failed and the device is still streaming — its NAK is discarded), the camera is left at offset (0,0) with the *old* width/height. `roi_rejected` is logged and returned as NAK but nothing rolls back, and `CameraSetup` (`wiringPanel.cpp:2753`) only records the name in `g_camSetupFailed` and carries on — then pushes the *read-back* offset into the sampler (`:2755-2762`), so lens-calib/mmpp lookups are silently biased to the wrong origin. Same discarded-status pattern at `wiringPanel.cpp:2579` (`StopAcquisition`) and `:2770` (`StartAcquisition`).

**6. Driver-side gap detector goes blind after every restart — `CameraLayer_HikRobot_Camera.cpp:171-179`**
`_lastFrameNum` / `_lastFrameNumValid` are never reset in `StartAcquisition()` (`:1182`). After a restart the counter is back near 0 while `_lastFrameNum` holds the old high value, so `nFrameNum &gt; _lastFrameNum + 1` is false for every frame until the counter climbs back past it — all real drops in that window are missed, and `_framesDroppedByGap` under-reports. Mirror-image of defect 3. Also `_lastFrameNum + 1` overflows at `UINT_MAX`, producing one bogus multi-billion `lost` at true wrap.

**7. `TriggerActivation` and `TriggerSelector` are never written — anywhere in the file**
Grep confirms only `logTriggerConfig` (`:823-836`) ever touches them, read-only. Both are inherited from the camera's persisted user set. A stored `TriggerActivation=FallingEdge` or a `TriggerSelector` other than the one `TriggerMode=On` is meant for makes the rig fire on the wrong edge or not at all, and the only trace is a `LOGI` numeric dump. `TriggerSource` is likewise left on `13` ("Anyway", `:962`), which the comment notes silences `Line0RisingEdge` events — so `_line0RisingEdges` reads 0 and the one independent trigger cross-check is dead by configuration.

**8. `synthPend` ring is written without a fullness check — `wiringPanel.cpp:1663-1666`**
Producer does `synthPend[h % 128] = {...}; head.store(h+1)` with no comparison against `tail`. The comment at `:1482-1484` claims "dropping the oldest under a flood"; what actually happens is the producer overwrites the slot the sender at `:1515` is concurrently reading — a torn, non-atomic 24-byte struct — and the consumer then sends a verdict with a mismatched `tid`/`cam_ts` on a real peripheral channel. Only reachable with `INSP_CAM_TS_SYNTH=1` (bench), but that is exactly where pairing measurements are taken.

**9. `CamInfo2Json` emits three fields under the wrong keys — `CameraLayerManager.cpp:14-16`**
`"vendor"` ← `info.model`, `"model"` ← `info.serial_number`, `"serial_nbr"` ← `info.vender`. Every camera list shown to the UI has vendor/model/serial rotated.

**10. Camera-open failure is swallowed silently — `CameraLayerManager.cpp:202-206` (and 181-184, 222-226)**
`catch (const std::exception &amp;ex) {}` with no log, then `return NULL`. The constructor's own diagnostics ("target device open/connect failed", "RegisterImageCallBack failed") are thrown as `invalid_argument` text and discarded here, so a wedged/busy camera is indistinguishable from "no camera present".

**11. `CameraSetup` always reports success — `wiringPanel.cpp:2581`, `:2771`**
`retV` is assigned in nine branches and never returned; `return 0` is unconditional. `LoadCameraSetting` (`:2888-2890`) propagates that 0, so a settings file that applied nothing looks identical to one that applied everything (only the racily-read `g_camSetupFailed` string carries the truth). Related: `SetRGain/SetGGain/SetBGain` (`CameraLayer_HikRobot_Camera.cpp:993-1018`) discard all four SDK returns and unconditionally `return ACK`, and `CameraSetup:2664-2677` discards even that.

## SUSPECTED

**12. `AcquisitionFrameRateEnable` is written to a streaming camera — `CameraLayer_HikRobot_Camera.cpp:921`**
It is written *before* the deliberate `StopAcquisition()` at `:935` that the adjacent comment says acquisition-control nodes require. On a live `TriggerMode(2)` switch the write is likely rejected, is only logged, and leaves the trigger-rate ceiling in force — the exact failure the comment at `:915-920` says must not happen. Move it inside the stop/start bracket.

**13. `SnapFrame` permanently changes the trigger configuration — `CameraLayer.cpp:47-54`**
`TriggerMode(1)`/`(2)` is applied and never restored on exit. A UI snapshot (type 0/1) while the machine is in hardware-trigger mode leaves the camera on `TriggerSource=Software` with `takeCount=0` (`CameraLayer_HikRobot_Camera.cpp:948`), and `ImageCallBack` early-returns on `takeCount==0` (`:348`) — hardware triggers then produce nothing until something re-issues `TriggerMode(2)`.

**14. Setting order in `CameraSetup` applies framerate against the pre-ROI sensor window — `wiringPanel.cpp:2644-2650` vs `:2753`**
`SetFrameRate` clamps to `resFPS.fMax` (`CameraLayer_HikRobot_Camera.cpp:1084`) read at the *full-sensor* geometry, then the 803x526 ROI is applied afterwards, which raises the achievable rate. Harmless with the bench's `framerate:-1` (uncapped short-circuit at `:1073`), live for any positive cap. `refreshExposureFloor()` is re-read on each start so `_floor_us` is fine.

**15. `ImageDownSampling` crop end is off by one — `wiringPanel.cpp:2337-2338`**
`X2 = xx + W` treats `X2` as an inclusive end everywhere else (`sxEnd = X2/downScale`, loops use `&lt;=`), so a W-wide crop yields W+1 source columns. Clamped to bounds, so it is a one-pixel geometry error, not a read overrun.

**16. `s_dev_list` global re-enumeration hazard — `CameraLayer_HikRobot_Camera.cpp:18`, `:469`, `:553-586`**
Already documented in the file header. Worth flagging only because `discover()` (`CameraLayerManager.cpp:49`) memsets it while previously-built `camBasicInfo` indices are still used by `connectCamera`; a second `discover()` between enumerate and construct gives the constructor stale `pDeviceInfo` pointers.

---

## Audit the core inspection pipeline

`task-id ad183e4115d4cbfe0`  8444 chars

Audit complete. Findings ranked most-severe first.

## CONFIRMED

**1. `DoImageTransfer=false` turns image streaming ON, at full rate — `wiringPanel.cpp:7828-7831`**
```
if(DoImageTransfer==false) { *skipImageTransfer=false; }
```
Sense inverted (`false` = "do send"). Worse, it sits *after* the FPS gate at :7822 (`if(withinMinInterval==false) *skipImageTransfer=true;`), so it also clears the rate limit. Scenario: operator sends `{"DoImageTransfer":false}` (:5946) to shed load on a struggling host; the preview instead starts sending an image for **every** frame with the max-FPS cap disabled — the opposite of the request, and the exact condition (:11160) that fills `datViewQueue` and starts dropping preview frames. Also breaks `DATA_VIEW_INSP_DATA_MUST_WITH_IMG` at :7834, which derives report-skip from this now-always-false flag.

**2. Uncounted, unbounded frame drop at the acquisition gate — `wiringPanel.cpp:7294-7299`**
```
if(inspQueue.size()&gt;imageQueueSkipSize) { LOGE("skip image, ..."); return CameraLayer::NAK; }
```
No counter, no rate limit on the LOGE. Every other drop site in this file has one (`poolEmptyDropCount` :7332, `inspQueueDropCount` :7484, `datViewDropCount` :11194). Live in CI mode only (`imageQueueSkipSize=1` at :5160; FI sets it to `capacity()` at :5133, and the default `-1` is inert because `size_t &gt; int` promotes `-1` to `SIZE_MAX`). Scenario: CI streaming with inspection ≥2 frames behind — parts pass unjudged, the operator sees no drop count in the `inspQueue` telemetry at :4307, and stderr floods at camera rate. Two counting layers below (`inspQueueDropCount`) also stay at zero because this returns before the queue is ever touched.

**3. Second `push()` unchecked → pool-slot leak with `_enqueued=true` — `wiringPanel.cpp:7489-7492`**
The retry `inspQueue.push(headImgPipe);` return is discarded, then `_enqueued = true` is set unconditionally. If it fails, the slot is neither queued nor returned to `resPool`, and the `catch` at :7536 won't recover it because `_enqueued` lies. Same shape at `:11200` (`datViewQueue.push(imgPipe)` retry, inside `doPassDown`, so the caller will not return it either). Each occurrence permanently shrinks the pool by one; ~`resourcePoolSize` occurrences and acquisition is dead with the camera still "running" — the exact failure the :7340 comment was written to prevent. Also uncounted.

**4. `perifSendQueue` drop accounting is wrong in both directions — `wiringPanel.cpp:10966-10970`**
```
perifSendQueue.pop(discard);   // return ignored
perifSendQueue.push(msg);      // return ignored
int n = ++perifSendDropCount;  // unconditional
```
Unlike the `inspQueue` site (:7481) this does not gate the counter on `pop()` succeeding. If the send thread drained the queue between the failed push and the pop, nothing was dropped but a drop is counted — and on a `PERIF_UINSP_MEGA` machine that emits `"POSITIONAL pairing: the verdict train is now OFF BY ONE"`, a false alarm about parts being mis-sorted. If the retry push fails, *two* verdicts are lost and one is counted. This counter is exported as `perif_pairing.link.queue_dropped`, i.e. it is the number an operator uses to decide whether to re-run a batch.

**5. The three pipeline threads have no exception handler — `wiringPanel.cpp:9902` (InspSnapSaveThread), `:10107` (ImgPipeDatViewThread), `:11235` (ImgPipeProcessThread)**
`SlowFrameSaveThread` (:9506) and `PerifSendThread` (:9894) both catch; these three do not. Anything thrown inside — `cv::Exception` from `saveInspectionSample`/`imwrite` in the snapshot thread (the codebase already added `safe_imwrite_cache` at :3811 precisely because imwrite throws on a bad path), `bad_alloc`, or `TS_Termination_Exception` from `pop_blocking` — escapes the thread function and is `std::terminate`. Scenario: snapshot target folder on a disconnected network share, imwrite throws, whole core dies mid-run. Related: `while (queue.pop_blocking(x))` never returns false, so `terminationflag` is only ever read once per thread — these threads cannot be stopped (masked today only because `mainLoop` ends in `_exit(0)`).

**6. Use-after-return of a pool slot still held as `lastDatViewCache` — `wiringPanel.cpp:7534` and `:11263`**
Both do a raw `bpg_pi.resPool.retResrc(headImgPipe)` on `!doPassDown`, bypassing `image_pipe_info_gc`/`_release` and therefore not testing `occupyFlag`. But `InspResultAction_s` sets the `resendCache` bit and stores the slot in `lastDatViewCache` at :8036 **without** setting `ret_pipe_pass_down`. So on the inline path (`doImgProcessThread==false`, :7530-7534) the slot goes back on the free list while `lastDatViewCache` still points at it; the camera then `create()`s over that buffer while `__LAST_DATA_VIEW_CACHE_IMG__` (:3867), `field_calib_capture` (:2157) or `LAST_FRAME_RESEND` (:6402) reads it under `lastDatViewCache_lock` — a lock that protects nothing here, since the writer is the capture thread and doesn't take it. Reachable only via the non-threaded path, which is why it hasn't bitten; the `:11217` comment documents the same hazard for the sibling site.

**7. `InspResultAction_s` emits its multi-packet batch as four independent fan-outs — `wiringPanel.cpp:7853, 7873, 8032, 8048`**
SS-start / RP / IM / SS-end are four separate `pushToSubscribers` calls, each retaking `subscribersLock`. `main.h` documents `pushBatchToSubscribers` as existing specifically because a torn batch leaves a peer's demux with an unterminated SS-start — and `pushCamStateDoorbell` uses it. This path does not. Two threads run this function concurrently (ActionThread per frame, and the WS thread on `LAST_FRAME_RESEND` at :6402 — the `thread_local test1_buff` comment at :7930 confirms both). `MT_LOCK` at :7791 is a documented no-op, so nothing serialises them: the resend's SS/IM interleaves with a live frame's SS/RP/IM on the same `CI_pgID`. `static int frameActionID` (:7787) and the `lastImgSendTime`/`avgInterval` globals (:7783-7784, written at :8034-8035) are likewise raced by those two threads.

**8. `RingBuff.cpp` is dead, broken, and duplicates the header**
`RingBuff.cpp:79` defines a free function `size()`, not `RingBuf::size()`; `:92`/`:101` use unqualified `RB_Idx_Type` outside class scope; the templates repeat default template arguments (ill-formed). It is not in any `add_library`/`add_executable` in `InspectionCore/CMakeLists.txt` — the real definitions are inline in `common_lib/include/RingBuf.hpp`. Anyone "fixing" a ring-buffer bug here changes nothing. Its `pushHead()` at `:170` also does `tailLock.unlock()` unconditionally from a thread that never locked it — the same UB the TSQueue and `resourcePool` rewrites were done to purge.

## SUSPECTED

**9. Unsynchronised read of `lastDatViewCache` — `wiringPanel.cpp:1059`** — `if(lastDatViewCache==&amp;info)return true;` is read *before* `lastDatViewCache_lock` is taken on the next line, and on that early-return path the `resendCache` bit is never set. Same unlocked read at `:6392` and `:3939`. Benign on x86 in practice, but it is the one field whose staleness the whole cache-swap discipline rests on.

**10. `calib_bacpac` is one global shared by two threads — `wiringPanel.cpp:7420` / `:10336`** — every pipe gets `bacpac = &amp;calib_bacpac`, and the inspection thread writes `bacpac-&gt;cam = frameCam` per frame (:10336) while the datView thread reads `bacpac-&gt;sampler` in `ImageDownSampling` (:7975). The per-frame `camLayer` check at :10327 was added to stop a frame outliving its camera, but it fixes the *pipe's* copy and then publishes into shared state, so the guarantee is only as good as there being one reader.

**11. Snapshot filenames collide silently — `wiringPanel.cpp:10035`** — `filePath = ... + std::to_string(current_time_ms())`, millisecond resolution. Two NG parts in the same millisecond overwrite each other; `saveInspectionSample` returns 0 and the rate-limited "snapshot saved" line at :10093 reports success. NG evidence lost with no counter, on a path whose stated purpose is not losing NG evidence.

**12. `saveInspQFullSkipCount` is a plain `int` across three threads — declared `:315`, incremented `:8068` (ActionThread), read `:4327` and reset to 0 `:5006` (WS thread)** — every sibling drop counter (`datViewDropCount` :412, `inspQueueDropCount` :417, `poolEmptyDropCount` :418) is `std::atomic&lt;int&gt;`; this one was missed. Lost increments undercount dropped snapshots.

Nothing was modified.

---

## Audit matching and pose transform

`task-id a05ade7af97fd480a`  8400 chars

Findings, most severe first.

## 1. Any non-finite measurement is reported PASS — CONFIRMED
`FeatureManager_sig360_circle_line.cpp:936-947`
```
if (judgeReport.measured_val &gt; USL || judgeReport.measured_val &lt; LSL) FAILURE; else SUCCESS;
```
`notNA` is set `true` on every path that ran, including ones that produce NaN. With NaN both comparisons are false, so the judge lands in the `else` branch: **STATUS_SUCCESS**. Sources of NaN reaching here: parallel-line intersection (`acvIntersectPoint`→NaN, line 710), NaN feature positions from a NaN-radius arc, NaN `end_pt`/`anchor`. Scenario: two nominally-crossing lines degenerate to parallel on a bad part → angle measure is NaN → part ships as in-spec. Also `USL_b/LSL_b` are selected when `flip_f&lt;0`; with back-side limits disabled these read as whatever the def defaults hold, so flipped parts are judged against possibly-unset limits.

## 2. Circle feature with a NaN radius reports SUCCESS — CONFIRMED
`FeatureManager_sig360_circle_line.cpp:4704-4712`. When the caliper fit fails, `cf.circle.radius = NAN` is set explicitly (`:4448`), then the same NaN-vs-range test falls through to `STATUS_SUCCESS` with a NaN centre and radius. Those NaN values then flow into `ParseLocatePosition` (`:594`, which only rejects `STATUS_NA`) and into every downstream distance/angle/aux-point.

## 3. Collinear / zero-radius arc definition is never rejected — CONFIRMED
`convert3Pts2ArcData` (`:195-232`) calls `acvCircumcenter`, which for collinear or coincident pt1/pt2/pt3 returns `(NaN,NaN)` via the parallel-line guard; `radius`, `sAngle`, `eAngle` all become NaN with no check. In `caliper_locate_circle` (`Caliper.cpp:552-556`) `span = angEnd-angStart` is NaN, both `while` loops are no-ops, every caliper anchor is NaN, all miss → `r.ok=false` → feeds directly into defect #2. A three-point arc drawn nearly straight silently becomes a passing "measurement".

## 4. `lineCross` aux point: SUCCESS with a NaN point — CONFIRMED (as briefed)
`:1023-1032`. `lineCrossPosition` returns 0 unconditionally after `acvIntersectPoint` (`:576`); `APointMatching_ReportGen` sets `STATUS_SUCCESS` and `rep.pt = cross`. The `centre` branch (`:1040`, `:1050`) has the same shape — it propagates whatever `ParseLocatePosition` handed back, including NaN from #2.

## 5. Back-only defs cannot match at all in the shape (SBM) path — CONFIRMED
`:7519` `modc.flip = (matching_face == 0);`
`matching_face` is tri-state: `0`=both, `1`=front, `-1`=back (the sig360 path handles it correctly at `:5513`/`:5515` with `&gt;=0` / `&lt;=0`). For `matching_face == -1` the SBM model is built **without** mirrored variants, so a back-only def never produces a match. Should be `matching_face &lt;= 0`.

## 6. `sig_match_sim_thres` is not applied in the shape path — CONFIRMED
`:7868` / `:7914`. `singleReport.similarity = m.score/100.0f` is stored with no comparison against `sigMatchSimThres` (nor `sigRelativeMatchSimThres`). The sig360 path gates twice (`:5648`, `:5802`); the SBM path accepts every match line2Dup returns above its own internal score floor. Raising `sig_match_sim_thres` in a def has zero effect once the def is on the shape locator.

## 7. `matching_angle_margin` / `matching_angle_offset` are ignored in the shape path — CONFIRMED
`:7511-7519`: `modc.angle.start = 0; modc.angle.end = 360;` always, and the accepted `m.angle` is never tested against the def's margin/offset anywhere in `SingleMatching_shape_all`. The `SHAPE_ANG_RANGE` env hook at `:7515` documents this ("to test a constrained angle search **before wiring it to the def margin**"). A def that restricts orientation to ±10° will happily accept a part rotated 170°.

## 8. Similarity is square-rooted twice in the per-candidate gate — CONFIRMED
`:5793` `error = sqrt(error);` then `:5801` `1 - SigMatchErrorNormalize(error, …)`, and `SigMatchErrorNormalize` itself does `sqrt(error)/sig.mean` (`:4145`). So the retry-loop gate computes `1 - sqrt(sqrt(err))/mean` while the early gate at `:5648` computes `1 - sqrt(err)/mean` from the same quantity. `sigMatchSimThres` therefore means two different things at the two gates, and `singleReport.similarity` (`:5807`) — the number the UI shows and operators tune against — is the double-sqrt one. For `err&lt;1` the second gate is strictly stricter, silently rejecting candidates the first gate passed.

## 9. Line direction for flipped parts disagrees between the two locating modes — CONFIRMED
- Caliper mode: `LineMatching_caliper` swaps `p0/p1` when `flip_f&lt;0` and then **negates `line_vec` back** at `:4901-4905`, so the reported vector follows the un-mirrored def convention.
- Contour mode: `SingleMatching_line` orients `line_cand.line_vec` to `target_vec` (`:3118`), which is the already-pixel-domain, already-mirrored `p1-p0`.

The two paths return opposite-signed `line_vec` for the same flipped part. Every `anglefollow` search point and every ANGLE/DISTANCE judge that hangs off that line changes sense when the operator toggles `locating` on a flipped part. One of the two is wrong; they cannot both be.

## 10. Search direction negation is applied only to non-flipped parts — SUSPECTED (strong)
`:1100` in `searchPoint_process`:
```
if (flip_f &gt; 0) vec = acvVecMult(vec, -1);
```
The angle sign is already mirror-corrected two lines up (`:1085 angle = -angle`), which is the correct handling. A plain negation `v → -v` commutes with the mirror (`F(-v) = -F(v)`), so it must be applied in **both** cases or neither. As written, a flipped part searches its edge from the opposite side, which combines with `search_far` (`:1113`) to invert the scan. This may be an accidental cancellation against defect #9's caliper-path negation, in which case one of the two is the bug — either way the pair is not self-consistent.

## 11. Flipped candidates: the angle gated is not the angle posed — SUSPECTED
`:5566` gates back candidates on `minMatchErr_bk[i].x*π/180 + M_PI` (the "Y-axis flip" convention), but `:5783` `angle = minErr.x * M_PI / 180;` uses the raw value, and that is what becomes `singleReport.rotate` (`:5820`). The two conventions describe the same physical pose, but the number reported to consumers is 180° away from the one `matching_angle_offset` is expressed in. Setting a non-default `matching_angle_offset` gates front and back matches against inconsistent reference frames.

## 12. Angle wrap normalization is asymmetric — CONFIRMED
`:742-747`
```
if (angleDiff &lt; -M_PI) angleDiff += 2 * M_PI;
if (angleDiff &gt;  M_PI) angleDiff -=     M_PI;   // &lt;-- should be 2*M_PI
```
The positive branch subtracts π, not 2π. An `angleDiff` of, say, 3.0 rad maps to −0.14 rad instead of staying 3.0. This lands upstream of the `angleDiff_mid` correction block, so the reported ANGLE measurement is wrong (by π) over roughly a quarter of the input domain. `:8074` in the shape path has the matching `+= 2*M_PI` — only this site is wrong.

## 13. FAILURE-status features are consumed as valid geometry — CONFIRMED
`ParseLocatePosition` (`:582-640`) and `ParseMainVector` (`:468-520`) reject only `STATUS_NA`. A circle whose radius fell outside `initMatchingMargin` (`:4708`, `STATUS_FAILURE`) still hands its centre to any dependent aux point, distance, or angle, which then reports SUCCESS. `STATUS_UNSET` is likewise accepted, returning stale/uninitialized coordinates if the feature was never executed.

## 14. `isAngleInRegion` returns whether the angle is *outside* the region — CONFIRMED (latent)
`:4129-4141` ends with `return (angle &gt;= to);` after normalizing — i.e. true means outside `[from,to]`. The two call sites (`:5537`, `:5571`) use it correctly given that behavior, so the current code works, but the name asserts the opposite of what it does and the debug string at `:5551` reads consistently with the name, not the code. Any third caller will be wrong.

## 15. Exclude polygons are dropped when no mask was built — SUSPECTED
`:7147` `if (!loc_excl_mm.empty() &amp;&amp; !mask.empty() &amp;&amp; def_mmpp &gt; 0)`. If no include polygon, no signature, and no Otsu blob produced a mask, `mask` stays empty (meaning "use all features") and `localization_exclude` is silently ignored — features are extracted from exactly the areas the operator marked as avoid. Relatedly, the include mask is dilated 11px at `:7057` before exclusion, so the include region grows ~5px past its authored boundary.

---

## Audit the live inspection UI

`task-id af858353816d9c52b`  8079 chars

Findings below. No files modified.

## Ranked defects

**1. Parts silently dropped from all statistics — off-by-one between the two tracking-window filters. CONFIRMED**
`redux/reducer/UICtrlReducer.js:264` (expiry) requires `repeatTime &gt; statSetting.minReportRepeat`, while the noise filter at `:702-704` keeps entries with `repeatTime &gt;= statSetting.minReportRepeat`. So with CI defaults (`minReportRepeat:2`, `headReportSkip:1`, info.js:24-26) an entry seen exactly `minReportRepeat` times survives in the window, then on expiry falls into the `else` at `:309` — logged as `log.error("the current data only gets few samples, ignore")` and discarded. It never reaches `statReducer`, `historyReport`, `newAddedReport`, or the DB. Worse, `headSkipTime` resets `repeatTime` to 0 at the second sighting (`:663-670`), so a part actually needs ~4 sightings to be counted at all. Failure: a fast part / brief occlusion is inspected and judged, and vanishes from every operator-facing count with no drop counter anywhere. There is no counter of these discards at all.

**2. DB upload loses whole batches of completed parts under the 100 ms action throttle. CONFIRMED**
`redux/middleware/ActionThrottle.js:57-75` queues report actions and flushes them all synchronously in one tick (`:44-53`). The reducer *overwrites* `reportStatisticState.newAddedReport = []` on every report (`UICtrlReducer.js:247`). `InspectionUI.js:166-197`'s `useEffect([newAddedReport])` sees only the final array after the batch, so every completed part that landed in an intermediate `newAddedReport` during the same flush is never sent to the DB and never counted. At 25-40 reports/s (the rate the file's own comments cite) that is most flushes. Corollary: `_this.totalCounter` (`:181-182`) counts *renders*, not reports, so the `sended&lt;send:total/skip` string on the connect button (`:229`) systematically under-reports.

**3. `insert_skip` of 0 or undefined disables DB upload entirely, silently. CONFIRMED**
`InspectionUI.js:181`: `let res = _this.totalCounter % insert_skip; if(res!=0) return;`. `insert_skip` defaults to `0` in the signature (`:130`) and comes from `System_Setting.CI_MODE_UPLOAD_SKIP` / `FI_MODE_UPLOAD_SKIP` (`:3642-3643`), which is merged from the machine's `SystemSetting` (`comm/BPG_WS.js:307-311`) with no validation. `x % 0` and `x % undefined` are both `NaN`, `NaN != 0` → **every** report skipped. `0` is the obvious way for a machine config to mean "don't skip". The comment at `:3639` claims a default is applied when undefined; no such fallback exists. The button still reads "connected".

**4. Reducer crashes on a report whose sub-arrays don't line up. CONFIRMED**
`UICtrlReducer.js:547`, `:576`, `:609`: `singleReport.detectedLines.find(...)` etc. can return `undefined`, and the very next line dereferences `slrep.status` / `screp.status` with no guard — unlike the `judgeReports` merge at `:626`, which *does* check `sjrep === undefined`. Any id present in the tracked entry but absent in the new report (or a missing `detectedCircles`/`searchPoints` array) throws inside the reducer, taking down report ingestion / hitting the error boundary mid-run.

**5. `shouldComponentUpdate` swallows the WS-disconnect exit and the CI idle watchdog. CONFIRMED**
`InspectionUI.js:3197-3227` returns `isReportInc &amp; doUpdate` — a re-render happens only when `reportStatisticState.reportCount` changed *and* the image changed. But `componentDidUpdate` (`:3178-3196`) is the only place that calls `EXIT()` on `uInsp_API_ID_CONN_INFO`/`CAM1_ID_CONN_INFO` leaving `WS_CONNECTED`, and the only caller of `checkAutoExitForCI`. When the inspection WS drops, reports stop → `isReportInc` is false → no render → `componentDidUpdate` never runs → the screen never exits and the auto-exit watchdog never ticks. A conn-info prop change alone cannot get through this gate.

**6. Stale overlay pinned to a departed part. SUSPECTED**
`EverCheckCanvasComponent.js:1385` sets `this.frameReportList` only on the `updateImgOnly==false` branch (i.e. only when a NEW image arrives), and `:1710` prefers it forever once set. Images are throttled to ~6 fps while reports are not, and `InspectionUI.js:1872-1873` skips `EditDBInfoSync` entirely when `__surpress_display` is set and the image is unchanged. If the part leaves and no further image arrives, the last frozen list keeps being drawn — boxes/verdicts over an object no longer there. Nothing ever clears `frameReportList` on an empty tracking window.

**7. `resourceClean()` removes no listeners and leaks the decoded frame. CONFIRMED**
`EverCheckCanvasComponent.js:321` and `:1192`: `removeEventListener('wheel', this.onmouseswheel.bind(this))` — a fresh bound function is a different reference, so the listener registered at `:275` is never removed. The three touch listeners (`:261-269`) are never removed either. Additionally the INSP override at `:1191-1195` does **not** call `releaseRawImg()` (the base version at `:322` does), so the last `ImageBitmap`/`VideoFrame` native memory is never `close()`d when leaving the inspection screen.

**8. Diagnostic probe is installed unconditionally in production. CONFIRMED**
`redux/redux.js:168` calls `installDiagProbe(store)` with no DEV gate. That permanently installs, on the live screen: a per-frame rAF loop (`diagProbe.js:137-138`), a 250 ms `setInterval` that is never cleared (`:174`), a whole-document `MutationObserver` whose handler recursively walks each added/removed subtree node-by-node (`:246-262`) — on the screen that churns DOM per report — and monkey-patches `Document.prototype.createElement`/`createElementNS`/`createTextNode`/`createDocumentFragment`/`cloneNode` with a try/catch tally on every call (`:296-311`). This is main-thread cost added to precisely the path that stall-detection exists to protect.

**9. `_streamDS` believes it knows the core's state across a reconnect. SUSPECTED**
`InspectionUI.js:1820-1832`: `if (down_samp_level === prev) break;` where `prev = this._streamDS`. Nothing resets `_streamDS` when the core restarts or the CI stream is re-opened; the core comes back at its own default. If the UI remembers 4 and the core is at 1, no `ST` is ever sent and the live view floods at full resolution indefinitely (the exact regression the comment block describes). Also, the deadband only covers the 4 and 2 candidates — there is no hysteresis at the 2↔1 boundary (`:1821-1824`), so a canvas sitting near oversample 1.8 re-negotiates on every resize.

**10. Redux state mutated in place throughout the ingestion path. CONFIRMED (design-level)**
`UICtrlReducer.js:241-316` and `:480-700` mutate `reportStatisticState` (`reportCount++`, `historyReport.push`, `statisticValue` via `statReducer`) and mutate `trackingWindow` *entries* in place, plus `inspReport.time_ms = currentTime_ms` mutates `action.data` (`:211`). Only a shallow copy is taken at the end (`:717`), and two `break` paths (`:231`, `:327`) exit before even that. Consequence beyond style: any consumer holding a reference to a tracking entry sees it change under it — which is exactly the bug `EditDBInfoSync` had to deep-clone around (`EverCheckCanvasComponent.js:1364-1385`), and `EverCheckCanvasComponent.js:2860` still reads that live mutable list directly.

**11. Operator-facing send counters live on a `useRef` and are never a render trigger. CONFIRMED**
`InspectionUI.js:133`, `:229`: `sendCounter`/`sendedCounter`/`totalCounter` are mutated on a ref, so the button text only refreshes when the unrelated 2 s `dbQ` poll (`:164`) fires. The numbers an operator reads are up to 2 s stale, and a failed WS send is swallowed by the empty `.catch(err=&gt;{})` at `:194-196` — it increments nothing and `onDBInsertFail` is never called despite being passed in at `:3758`.

**12. `GetObjElement` throws on a null mid-path. CONFIRMED, low**
`UTIL/MISC_Util.js:58-72` only guards `=== undefined`; `obj = obj[key]` on a `null` intermediate throws. It is used on report/conn-info paths (`UICtrlReducer.js:823-824`, `InspectionUI.js`) where a JSON `null` from the core is plausible.

---

## Analyse aux/composite features

`task-id af2dabaf0e2eb4fec`  7293 chars

## Findings (most severe first)

### 1. UI's aux_point overlay is recomputed locally and never uses the core's reported point — CONFIRMED
- Core emits the aux point: `InspectionCore/MatchingEngine/FeatureReport_UTIL.cpp:273-292` (`auxPoints[] = {status,id,name,x,y}`).
- WebUI reads only the *status* of it: `UI/WebUI/src/UTIL/InspectionEditorLogic.js:1086-1100` (`FindInspShapeObject`, list `'auxPoints'`) and `:1102-1130` — `ShapeAdjustsWithInspectionResult` has **no `case aux_point`**, so `x/y` are discarded.
- Drawing instead re-derives the intersection in JS: `UI/WebUI/src/shapes/aux_point.js:110` and `:82` → `auxPointParse` (`InspectionEditorLogic.js:1426-1450`).
Two independent implementations of the same geometry, so any divergence (next item) shows up as an operator seeing a crosshair in one place while the measurement was taken somewhere else.

### 2. Flip: UI aux geometry disagrees with the core on mirrored parts — CONFIRMED
- Core, `FeatureManager_sig360_circle_line.cpp:507-512`: for a `search_point` ref, `angle = angleDeg*π/180; if (flip_f &lt; 0) angle *= -1;`
- UI, `InspectionEditorLogic.js:1500-1505` (`shapeVectorParse`, `search_point` case): `angle = atan2(lineVec) + shape.angleDeg*π/180` — **no flip term**.
Scenario: a part inspected flipped (`isFlipped`), aux_point = line × search_point. The core intersects with the angle mirrored; the UI intersects with the un-mirrored angle. The measure value is the core's, the drawn aux point and the distance indicator are the UI's — they point at different places, and the operator "verifies" against the wrong geometry. Same divergence exists in `ParseMainVector` for the ANGLE measure overlay.

### 3. Core reports an aux_point as SUCCESS with a NaN position — CONFIRMED
`FeatureManager_sig360_circle_line.cpp:1020-1034` (`APointMatching_ReportGen`, lineCross): `lineCrossPosition` returns 0 whenever both refs *located*, and `acvIntersectPoint` (`common_lib/vis_geom.cpp:43-53`) returns `(NAN,NAN)` for parallel/degenerate lines. The return value is not checked for finiteness, so `rep.status = STATUS_SUCCESS; rep.pt = NaN`.
Scenario: two nominally-parallel edges (or the same feature picked twice — nothing in the def format forbids it after an edit; the UI's ADD gate at `DefConfUI.js:2776-2778` only guards *creation*). JSON prints non-finite numbers as `null` (`FeatureReport_UTIL.cpp:288-289`), so the report says the aux point succeeded and has `x:null`. A dependent distance measure only escapes a bogus verdict because of the late `isfinite` guard at `:925`; the aux row itself reports OK.
Related, same function: `TreeExecution` at `:5219-5232` computes `id1_status`/`id2_status` for both refs and **discards them** — dependency status is only enforced indirectly via `ParseLocatePosition`'s `-2`.

### 4. `aux_line` render crashes on a null ref — CONFIRMED
`UI/WebUI/src/shapes/aux_line.js:37-49`: `subObjs` is built by `FindShape(...)` then mapped to `null` on a miss, but the guard is `subObjs.length == 2` (always true for a 2-slot ref) before `subObjs[0].pt1.x`. Any ref that isn't in the passed `shapeList` → TypeError inside the render loop → error boundary swallows the editor. Reachable whenever the render list is a *filtered* clone (e.g. the rank filter, `EverCheckCanvasComponent.js:1852-1857`) or a ref is missing. `aux_point.js:88` got this right (`refFoot` null-checks); `aux_line` did not.

### 5. `auxPointParse` returning `undefined` reaches an unguarded `distance_point_point` — CONFIRMED
`InspectionEditorLogic.js:1355-1362` (`FindClosestCtrlPointInfo`) and `:1383-1386` (`FindClosestInherentPointInfo`) pass the parse result straight into `distance_point_point` (`UTIL/MathTools.js:2-5`, no guard). `auxPointParse` returns `undefined` on any unresolved ref (`:1403`, `:1429`, `:1431`, `:1437`, `:1443`) and `undefined` for a `keyTrace` that misses. Mouse-move over the canvas then throws — exactly the failure class of the sig360 empty-list bug.

### 6. Ref-tree walks deref `shape.ref` when only `ref_baseLine` exists — CONFIRMED (reachability SUSPECTED)
`InspectionEditorLogic.js:936-940` (`findLostRefShapes`) and `:957-959` (`FindShapeRefTree`): the early-out is `if (shape.ref === undefined &amp;&amp; shape.ref_baseLine === undefined) return false;` — an `&amp;&amp;`, so a shape with `ref_baseLine` but no `ref` falls through to `[...totalRef]` / `shape.ref.find(...)` and throws. That kills def load (`:812`) or the delete-cascade dialog (`DefConfUI.js:2849`). Distance measures currently always get `ref:[{},{}]` (`:588`), so this needs a hand-edited/legacy def to fire.

### 7. Inherent aux points are drawn from def geometry during inspection — CONFIRMED
`shapes/aux_point.js:133-135` (`drawInherent`) calls `renderer.db_obj.auxPointParse(shape)` **without a shapeList**, so it defaults to `this.shapeList` (the def), never the inspection-adjusted clone. Every `&lt;arc&gt;.centre` inherent marker and the signature centre therefore render at the taught position while everything around them renders at the measured position.

### 8. Smaller, all CONFIRMED
- `searchPointParse` (`InspectionEditorLogic.js:1461`) ignores its own `shapelist` parameter — `FindShapeObject("id", ref[0].id)` with no list — so aux resolution against a cloned/adjusted list silently consults the def list.
- UI `intersectPoint` (`UTIL/MathTools.js:111-127`) has **no zero-denominator guard** (the core added one at `vis_geom.cpp:51`): near-parallel aux refs yield a finite-but-enormous garbage point, exactly-parallel yields `±Infinity`/`NaN`, and callers treat it as a valid point.
- Lost-ref pruning at `:810-815` is single-pass: dropping a shape whose refs are lost can orphan a *second* shape (e.g. a measure on the pruned arc's inherent `.centre`), which is not re-checked and ships in the def; the core then finds no report index and the measure is NA forever.
- `UpdateInherentShapeList:900-906` mints inherent ids as `100100 + shape.id*10` — collides with the signature block / other inherents once a user shape id reaches ~10000, and the `@__SIGNATURE__.orientation` `aux_line` ref (`:888-895`) carries `name`+`keyTrace` but **no `id`**, so `findLostRefShapes` always classifies it as lost (harmless only because the filter compares object identity against `shapeList`).

### On the reference model (answers the scoping question)
References are **by id**, consistently, on both sides (`ref[i].id` written at `InspectionEditorLogic.js:441-444`/`:474-477`, parsed at `FeatureManager_sig360_circle_line.cpp:1530-1546`); ids are stable across reorder, and deletion cascades through `FindShapeRefTree`/`FlatRefTree` (`:954-1005`). The one positional dependency is the **ref array slot**: the core picks the aux subtype purely from `JFetch_OBJECT(jobj,"ref[1]") != NULL` (`:1530`), and reads `ref[0].id`/`ref[1].id` with `JFetEx_NUMBER`, which *throws* on a missing key — so an aux_point whose slot was emptied by an edit turns into either a silently different subtype (centre instead of lineCross) or a hard def-parse rejection, not a validation error. Cycles are prevented only at pick time in the UI (`DefConfUI.js:2333-2346`, `refChainHasLoop`, which does not follow `ref_baseLine`); the core's defence is the in-progress `STATUS_NA` stamp in `TreeExecution` (`:5153`, `:5175`, `:5217`).

---

## Analyse measurement and limits

`task-id a5a69ec9647c34aac`  6263 chars

## Defects (most severe first)

### 1. CONFIRMED — `couple_value_b` centres the BACK limits on the FRONT target
`UI/WebUI/src/shapes/measure/index.js:55-59`
```js
const couple_value_b  = (obj, prev) =&gt; { if (obj.value === undefined) return;
  obj.LCL_b = round(obj.LCL_b - prev + obj.value, 0.001); ... obj.USL_b = round(obj.USL_b - prev + obj.value, 0.001); };
```
Every line uses `obj.value` where it must use `obj.value_b` (compare `couple_value` at :50-54, which is correct, and `couple_LSL_b`/`couple_USL_b` at :64-67 which correctly use `value_b`). The guard is also on the wrong field.
Scenario: front value 10, `back_value_setup` on seeds value_b=10, LSL_b/USL_b = 9/11. Operator types value_b = 20. Expected 19/21; actual `9 - 10 + 10 = 9` and `11 - 10 + 10 = 11` — the back limits do not move at all. The back-side spec window is now nowhere near the back target, the core judges flipped parts against exactly those `USL_b/LSL_b` (`FeatureManager_sig360_circle_line.cpp:936-938`), and every flipped part is physically rejected while the number on screen reads as configured. Any value_b edit ≠ value produces this silently.

### 2. CONFIRMED — the roll-up loses an NG produced by `NAasNG` (screen says OK, core rejects)
`UI/WebUI/src/UTIL/InspectionEditorLogic.js:75-98` + `UI/WebUI/src/InspectionUI.js:1372`
`MEASURERSULTRESION_reducer` makes the accumulator sticky only for `NA`, `USNG`, `LSNG`, `UCNG`, `LCNG`. `SNG` and `NG` fall through to the last line `return measure_result_region` — i.e. an accumulated `NG` is overwritten by the next item's status.
`InspectionUI.js:1372` is the one place that feeds `NG` into that reducer: `if (rdef.NAasNG &amp;&amp; st === NA) st = NG;`. So for a def with `NAasNG`, the NG survives only if that measurement happens to be the LAST essential item in the list; any later `UOK` resets the part to OK. The core (`wiringPanel.cpp:7253-7280`) converts the same item to `STATUS_FAILURE`, which `InspStatusReducer` never loses. Part is sorted as reject, panel shows a green verdict. `__gradeMismatch` cannot see it — it compares ITEM status only.

### 3. CONFIRMED — first-tag vs last-tag: the screen and the core use different 製程 overrides
`UI/WebUI/src/InspectionUI.js:2748` picks the override set with `.find(tag =&gt; ctrlMarginInfos[tag] !== undefined)` — the FIRST matching tag — and folds it into `shape_list`, which is what the wire def sent to the core is built from.
`UI/WebUI/src/redux/reducer/UICtrlReducer.js:332-339` `MarginInfoExtraction` reduces over the same tag array keeping the LAST match, and `resultGrading` overlays it on `shapeList` (`:439`, `:651`, `:673`).
Scenario: a part carries tags `["procA","procB"]`, both with `control_margin_info` rows. Core judges against procA's limits; the UI grades and colours against procA-merged-with-procB (procB winning per field). Divergent by construction, and it is order-dependent on the tag array. Same class also makes the mismatch counter fire without anyone changing a limit.

### 4. CONFIRMED — per-製程 limit overrides silently do not apply to flipped parts
`UI/WebUI/src/DefConfUI.js:828` — `PERSIST = ['id','rank','value','USL','LSL','UCL','LCL','quality_essential']`; no `value_b/USL_b/LSL_b/UCL_b/LCL_b`. The editor has no back-side columns either (`:645-676`).
The merge is `{...shape, ...info}` (`InspectionUI.js:2783`), so a 製程 row that tightens `USL` 10.0 → 9.5 leaves `USL_b` at the root 10.0; `effectiveLimits` (`InspectionEditorLogic.js:33`) then prefers `USL_b` for a flipped part, and the core does the same. On a def with `back_value_setup` on, the whole 製程 override is a no-op for every flipped part, with nothing on screen saying so — the operator sees the tightened number in the editor table.

### 5. CONFIRMED — the root row's `quality_essential` tristate is a dead control
`UI/WebUI/src/DefConfUI.js:871-897`. The root row object is built with `id/name/subtype/key/value/rank/USL/LSL/UCL/LCL` — no `quality_essential` — and its `update` closure writes back only `value,rank,USL,LSL,UCL,LCL`. The tristate renderer (`:745-775`) is nevertheless rendered on that row and calls `objInfo.update(new_obj)`. Clicking 是/否 on a root row is accepted by the UI and discarded; the button also always draws as “inherit” afterwards. `quality_essential` is the single field that decides whether a measurement counts, so this is “I disabled that measurement and it still rejects parts”.

### 6. SUSPECTED — an undefined/NaN limit passes everything, on both sides, with no diagnostic
`UI/WebUI/src/UTIL/InspectionEditorLogic.js:656-671`: all four tests are `&lt;`/`&gt;`, so if `USL`/`LSL` is `undefined` or NaN every comparison is false and the function returns `UOK` (green pass). A 製程 override row is written by `cleanRow` which drops undefined keys, so this normally inherits — but a root shape created outside `Shape_Attr_Fill` (or a hand-edited def) with a missing `USL` grades as a pass rather than as NA. Note the core protects the NaN *value* case explicitly (`FeatureManager…:924-931`) but has no equivalent guard for a NaN *limit*.

### 7. SUSPECTED — histogram bins collapse when the target sits on a limit
`UI/WebUI/src/UTIL/InspectionEditorLogic.js:209-210` sets `xmin/xmax` from `1.2*(LSL-value)+value` / `1.2*(USL-value)+value`. If `USL==LSL==value` (a not-yet-configured measure), `xmax==xmin`, and `spcStats.js:22` divides by `(xmax - xmin) == 0` → `val_idx` NaN → `histo[NaN+1]++` creates a stray `"NaN"` property; every sample is lost from the histogram with no error.

### Boundary check (no defect found)
Exactly `== USL` / `== LSL` is a PASS on both sides: UI uses `value &gt; USL` / `value &lt; LSL` (`InspectionEditorLogic.js:656-661`), core uses `measured_val &gt; USL || measured_val &lt; LSL` (`FeatureManager_sig360_circle_line.cpp:938`). `UCL/LCL` exist only in the UI (the core parses no control limits at all), so the amber band cannot diverge from the sorter. NA-absorbs-NG also matches between `MEASURERSULTRESION_reducer` and `InspStatusReducer`.

Minor note: `MEASURERSULTRESION_priority` (`InspectionEditorLogic.js:55-73`) is exported and imported nowhere — the actual reducer is the hand-written cascade in defect 2, so the priority table is not the specification it looks like.

No files were modified.
