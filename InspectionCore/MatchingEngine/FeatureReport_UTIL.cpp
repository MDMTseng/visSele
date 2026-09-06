#include <map>
#include <string>
#include "FeatureReport_UTIL.h"
#include "FeatureReport.h"
#include "FeatureManager_sig360_circle_line.h"
#include "FeatureManager_stage_light_report.h"

#include <logctrl.h>
#include <cmath>

cJSON* acv_acv_XY2JSON(acv_XY pt )
{
  cJSON* xy = cJSON_CreateObject();
  cJSON_AddNumberToObject(xy, "x", pt.x);
  cJSON_AddNumberToObject(xy, "y", pt.y);
  return xy;
}

float round_dec(float num, int round_mult)
{
  return roundf(round_mult * num) / round_mult;
}

cJSON* acv_LineFit2JSON(cJSON* Line_jobj, const acv_LineFit line, acv_XY center_offset )
{
  cJSON_AddNumberToObject(Line_jobj, "matching_pts", line.matching_pts);
  cJSON_AddNumberToObject(Line_jobj, "s", line.s);
  if (line.confidence > 0) cJSON_AddNumberToObject(Line_jobj, "confidence", line.confidence);

  /*acv_XY point = acvClosestPointOnLine(line.end_pos, line.line);
  cJSON_AddNumberToObject(Line_jobj, "x0", point.x-center_offset.x);
  cJSON_AddNumberToObject(Line_jobj, "y0", point.y-center_offset.y);
  point = acvClosestPointOnLine(line.end_neg, line.line);
  cJSON_AddNumberToObject(Line_jobj, "x1", point.x-center_offset.x);
  cJSON_AddNumberToObject(Line_jobj, "y1", point.y-center_offset.y);*/
  cJSON_AddNumberToObject(Line_jobj, "cx", line.line.line_anchor.x-center_offset.x);
  cJSON_AddNumberToObject(Line_jobj, "cy", line.line.line_anchor.y-center_offset.y);
  cJSON_AddNumberToObject(Line_jobj, "vx", line.line.line_vec.x);
  cJSON_AddNumberToObject(Line_jobj, "vy", line.line.line_vec.y);

  cJSON_AddItemToObject(Line_jobj,"pt1",acv_acv_XY2JSON(line.end_pt1));
  cJSON_AddItemToObject(Line_jobj,"pt2",acv_acv_XY2JSON(line.end_pt2));
  return Line_jobj;
}


cJSON* acv_CircleFit2JSON(cJSON* Circle_jobj,const acv_CircleFit cir , acv_XY center_offset)
{
  cJSON_AddNumberToObject(Circle_jobj, "matching_pts", cir.matching_pts);
  cJSON_AddNumberToObject(Circle_jobj, "s", cir.s);
  if (cir.confidence > 0) cJSON_AddNumberToObject(Circle_jobj, "confidence", cir.confidence);
  cJSON_AddNumberToObject(Circle_jobj, "x", cir.circle.circumcenter.x-center_offset.x);
  cJSON_AddNumberToObject(Circle_jobj, "y", cir.circle.circumcenter.y-center_offset.y);
  cJSON_AddNumberToObject(Circle_jobj, "r", cir.circle.radius);
  return Circle_jobj;
}

cJSON* JudgeReport2JSON(const FeatureReport_judgeReport judge , acv_XY center_offset )
{
  cJSON* judge_jobj = cJSON_CreateObject();
  cJSON_AddNumberToObject(judge_jobj, "status", judge.status);
  cJSON_AddNumberToObject(judge_jobj, "id", judge.def->id);
  cJSON_AddStringToObject(judge_jobj, "name", judge.def->name);
  cJSON_AddNumberToObject(judge_jobj, "value", judge.measured_val);

  switch(judge.def->measure_type)
  {
    case FeatureReport_judgeDef::ANGLE :
      cJSON_AddStringToObject(judge_jobj, "subtype", "angle");
    break;
    case FeatureReport_judgeDef::AREA :
      cJSON_AddStringToObject(judge_jobj, "subtype", "area");
    break;
    case FeatureReport_judgeDef::DISTANCE :
      cJSON_AddStringToObject(judge_jobj, "subtype", "distance");
    break;
    case FeatureReport_judgeDef::RADIUS :
      cJSON_AddStringToObject(judge_jobj, "subtype", "radius");
    break;
    
    case FeatureReport_judgeDef::CIRCLE_INFO :
      cJSON_AddStringToObject(judge_jobj, "subtype", "circle_info");

      switch(judge.def->data.CIRCLE_INFO.info_type)
      {
        case FeatureReport_judgeDef::data::CIRCLE_INFO::MAX_DIAMETER  :
          cJSON_AddStringToObject(judge_jobj, "info_type", "max_diameter");
        break;
        case FeatureReport_judgeDef::data::CIRCLE_INFO::MIN_DIAMETER  :
          cJSON_AddStringToObject(judge_jobj, "info_type", "min_diameter");
        break;
        
        case FeatureReport_judgeDef::data::CIRCLE_INFO::ROUGHNESS_MAX  :
          cJSON_AddStringToObject(judge_jobj, "info_type", "roughness_max");
        break;
        case FeatureReport_judgeDef::data::CIRCLE_INFO::ROUGHNESS_MIN  :
          cJSON_AddStringToObject(judge_jobj, "info_type", "roughness_min");
        break;
        case FeatureReport_judgeDef::data::CIRCLE_INFO::ROUGHNESS_RMSE  :
          cJSON_AddStringToObject(judge_jobj, "info_type", "roughness_rmse");
        break;

        default:
          cJSON_AddStringToObject(judge_jobj, "info_type", "NA");

      }
      


    break;
    case FeatureReport_judgeDef::SIGMA :
      cJSON_AddStringToObject(judge_jobj, "subtype", "sigma");
    break;
  }



  return judge_jobj;
}

cJSON* JudgeReportVector2JSON(const vector< FeatureReport_judgeReport> &judges , acv_XY center_offset)
{

  cJSON* judges_jarr = cJSON_CreateArray();

  for(int j=0;j<judges.size();j++)
  {
    cJSON_AddItemToArray(judges_jarr, JudgeReport2JSON(judges[j],center_offset) );
  }
  return judges_jarr;
}



// Emit a `cal_hits: [{x,y,st,s}]` array onto `parent` if `hits` is non-empty.
// Schema: x/y are image px minus center_offset (matches the line/circle anchor
// emit convention). st: 0=missed (no peak), 1=outlier, 2=inlier. s: per-caliper
// confidence (strength*unambiguity*sharpness). Missed entries still emit
// (status=0) so the WebUI can render a placeholder where a caliper tried and
// failed. Opt-in: contour-mode reports have hits.empty() so the field is absent.
// Runtime switch for the whole caliper-hit overlay. Default on.
//
// The hits are a by-product of a fit that has already happened, so keeping
// them in memory is nearly free -- what costs is exactly this function.
// Measured on the bench: ~300 hits per part, 85.4% of a 22 KB inspection
// record, i.e. 1200 doubles printed by cJSON at full precision plus a cJSON
// object per hit, twenty times a second. Every one of those bytes is also
// archived, because the record goes to the traceability DB whole.
//
// Gated HERE rather than at the producers because there are three of those
// (line/arc caliper, search-point, circle fit) and only one exit. Switching
// the overlay off therefore removes the serialisation, the wire bytes and the
// archive in one place.
//
// Deliberately all-or-nothing: a missed caliper (status 0) is as informative
// as a hit, so there is no "send only the ones that worked" mode.
// WHAT THE REPORT MAY CARRY BEYOND THE MEASUREMENT.
//
// One registry, one wire slot. Anything a human might want to look at while
// setting a recipe up -- caliper hits today, edge-strength profiles next --
// goes under the report's "extra" object and is named here. Two properties
// follow from that and are the reason for the shape:
//
//   * the archive strips ONE key. The traceability DB gets the record whole,
//     so every debug field added over the years would otherwise have to be
//     remembered and removed one by one. "extra" makes exclusion structural:
//     a payload added tomorrow is out of the archive by construction.
//   * an unknown name is ignored, not an error. A newer WebUI asking an older
//     core for a payload it has never heard of must degrade to "not sent",
//     never to a rejected command.
//
// Default OFF for anything expensive. cal_hits is the exception only because
// the inspection overlay has always drawn it; measured, it is 85% of a 22 KB
// record at ~300 hits a part, so it is the first thing to turn off on a line
// that does not need the overlay.
static std::map<std::string, bool> g_dbg_emit = {
    {"cal_hits", true},
    // The threshold-setting evidence. Off until a panel asks for it.
    {"edge_profile", false},
};

// DEBUG_EMIT=name[,name...] turns payloads on for a whole process, once, at
// first use. The ST command is the live switch and stays the one the WebUI
// uses; this exists because --insp has no command channel at all, so without it
// an offline run -- the CI check, a bench comparison, this file's own
// verification -- cannot see a payload that is off by default.
//
// A leading "-" turns one off, so a default-on payload can be dropped the same
// way: DEBUG_EMIT=-cal_hits,edge_profile
static void dbg_emit_env_once()
{
  static bool done = false;
  if (done) return;
  done = true;
  const char *e = getenv("DEBUG_EMIT");
  if (!e || !*e) return;
  std::string cur;
  std::string all(e);
  all.push_back(',');
  for (char c : all)
  {
    if (c != ',') { if (c != ' ') cur.push_back(c); continue; }
    if (cur.empty()) continue;
    bool on = true;
    if (cur[0] == '-') { on = false; cur.erase(0, 1); }
    auto f = g_dbg_emit.find(cur);
    if (f == g_dbg_emit.end()) LOGW("DEBUG_EMIT env: unknown payload \"%s\"", cur.c_str());
    else { f->second = on; LOGI("DEBUG_EMIT env %s=%d", cur.c_str(), (int)on); }
    cur.clear();
  }
}

bool DbgEmit(const char *name)
{
  dbg_emit_env_once();
  auto it = g_dbg_emit.find(name);
  return (it != g_dbg_emit.end()) && it->second;
}

// Returns the number of names understood, so the caller can log what it ignored.
int DbgEmitSet(cJSON *cfg)
{
  int taken = 0;
  if (!cfg || !cJSON_IsObject(cfg)) return 0;
  for (cJSON *it = cfg->child; it; it = it->next)
  {
    if (!it->string) continue;
    auto f = g_dbg_emit.find(it->string);
    if (f == g_dbg_emit.end())
    {
      LOGW("DEBUG_EMIT: ignoring unknown payload \"%s\"", it->string);
      continue;
    }
    f->second = cJSON_IsTrue(it);
    LOGI("DEBUG_EMIT %s=%d", it->string, (int)f->second);
    taken++;
  }
  return taken;
}

static void AddCalHits2JSON(cJSON *parent, const std::vector<CaliperHit> &hits, acv_XY center_offset)
{
  if (!DbgEmit("cal_hits")) return;
  if (hits.empty()) return;
  cJSON *arr = cJSON_CreateArray();
  // Appended with a tail pointer, not cJSON_AddItemToArray: that one walks
  // the list from the head on every call, so N hits cost N^2/2 pointer
  // chases. At the ~600 hits a search point may now report that is nothing;
  // at the tens of thousands it reported before the cap in SearchPointCV it
  // was the whole frame time.
  cJSON *tail = NULL;
  for (const CaliperHit &h : hits) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "x", h.pt.x - center_offset.x);
    cJSON_AddNumberToObject(o, "y", h.pt.y - center_offset.y);
    cJSON_AddNumberToObject(o, "st", h.status);
    cJSON_AddNumberToObject(o, "s",  h.strength);
    if (!tail) arr->child = o; else { tail->next = o; o->prev = tail; }
    tail = o;
  }
  // UNDER "extra", not beside the measurement. The archive strips that one key
  // (see UTIL/dbRecord.js in the WebUI), so nothing here is ever written to the
  // traceability DB no matter how many payloads are added later.
  cJSON *extra = cJSON_GetObjectItem(parent, "extra");
  if (!extra)
  {
    extra = cJSON_CreateObject();
    cJSON_AddItemToObject(parent, "extra", extra);
  }
  cJSON_AddItemToObject(extra, "cal_hits", arr);
}

// The across-edge gradient behind every caliper on this primitive, ungated.
//
//   edge_profile: { step, L, g: [[...],[...]] }
//
// g[i] is caliper i, in the same order as cal_hits, so the panel can pair a
// profile with the hit it produced. Sample j of a profile sits at
// (-L + j*step) px across the edge, measured from that caliper's centre along
// its search direction. Signed: the sign is the polarity the selector matches
// on (rising = dark->light), and folding it away would make polarity
// unsettable from the picture.
//
// A caliper that found NOTHING still emits its profile. That is the case the
// operator most needs to see -- "nothing passed the floor" and "there is no
// edge here" look identical in the measurement and completely different in the
// profile.
//
// Default OFF and it stays off outside recipe setup: at nAcross 67 this is
// ~670 numbers per primitive, which is an order more than cal_hits.
static void AddCalProfile2JSON(cJSON *parent, const CaliperProfiles &prof)
{
  if (!DbgEmit("edge_profile")) return;
  if (prof.grad.empty()) return;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddNumberToObject(o, "step", prof.step);
  cJSON_AddNumberToObject(o, "L", prof.L);
  cJSON *g = cJSON_CreateArray();
  for (const std::vector<float> &one : prof.grad)
    cJSON_AddItemToArray(g, cJSON_CreateFloatArray(one.data(), (int)one.size()));
  cJSON_AddItemToObject(o, "g", g);
  // Which sample the selector chose per caliper (-1 = none). The panel used
  // to guess this as "the strongest peak of the polarity", which is wrong
  // for first/last/nth and for a stronger neighbouring edge.
  if (prof.sel.size() == prof.grad.size())
    cJSON_AddItemToObject(o, "sel", cJSON_CreateFloatArray(prof.sel.data(), (int)prof.sel.size()));
  cJSON *extra = cJSON_GetObjectItem(parent, "extra");
  if (!extra)
  {
    extra = cJSON_CreateObject();
    cJSON_AddItemToObject(parent, "extra", extra);
  }
  cJSON_AddItemToObject(extra, "edge_profile", o);
}

// The search-point form of the same payload:
//
//   edge_profile: { kind: "peaks", span, p: [...], s: [...] }
//
// Entry i is one candidate: p[i] px along the search direction from the near
// end (so the first hit is the smallest p), s[i] its peak gradient. Ungated --
// including the ones the selector's own 0.40-of-the-strongest rule drops, which
// are exactly the ones a person needs to see to know whether the floor is
// doing the work or that rule is.
//
// Same key as the caliper form deliberately: one thing to ask for, one place to
// look, and `kind` says which shape arrived. A caliper profile has no `kind`.
static void AddSearchPeaks2JSON(cJSON *parent, const SearchPointPeaks &pk)
{
  if (!DbgEmit("edge_profile")) return;
  if (pk.pos.empty()) return;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "kind", "peaks");
  cJSON_AddNumberToObject(o, "span", pk.span);
  cJSON_AddNumberToObject(o, "mmpp", pk.mmpp);
  cJSON_AddItemToObject(o, "p", cJSON_CreateFloatArray(pk.pos.data(), (int)pk.pos.size()));
  cJSON_AddItemToObject(o, "s", cJSON_CreateFloatArray(pk.str.data(), (int)pk.str.size()));
  // a[i] is p[i]'s position along the bar: the pair makes the candidate cloud a
  // curve, which is what an apex can be fitted to.
  if (pk.along.size() == pk.pos.size())
    cJSON_AddItemToObject(o, "a", cJSON_CreateFloatArray(pk.along.data(), (int)pk.along.size()));
  // The point the scan actually returned, in the candidates' own frame.
  if (pk.sel_ok)
  {
    cJSON_AddNumberToObject(o, "sel_p", pk.sel_pos);
    cJSON_AddNumberToObject(o, "sel_a", pk.sel_along);
  }
  cJSON *extra = cJSON_GetObjectItem(parent, "extra");
  if (!extra)
  {
    extra = cJSON_CreateObject();
    cJSON_AddItemToObject(parent, "extra", extra);
  }
  cJSON_AddItemToObject(extra, "edge_profile", o);
}

cJSON* acv_CircleFitVector2JSON(const vector< FeatureReport_circleReport> &vec, acv_XY center_offset)
{

  cJSON* detectedCircles_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON* cfj = cJSON_CreateObject();
    cJSON_AddNumberToObject(cfj, "status", vec[j].status);
    cJSON_AddNumberToObject(cfj, "id", vec[j].def->id);
    cJSON_AddStringToObject(cfj, "name", vec[j].def->name);

    if(vec[j].status!=FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      acv_CircleFit2JSON(cfj,vec[j].circle,center_offset);

      cJSON_AddItemToObject(cfj,"pt1",acv_acv_XY2JSON(vec[j].pt1));
      cJSON_AddItemToObject(cfj,"pt2",acv_acv_XY2JSON(vec[j].pt2));
      cJSON_AddItemToObject(cfj,"pt3",acv_acv_XY2JSON(vec[j].pt3));
    }
    // Emit caliper-mode per-caliper hits even on STATUS_NA — the user wants to
    // see where the calipers tried and failed when the fit didn't converge.
    AddCalHits2JSON(cfj, vec[j].cal_hits, center_offset);
    AddCalProfile2JSON(cfj, vec[j].cal_prof);

    cJSON_AddItemToArray(detectedCircles_jarr, cfj );

  }
  return detectedCircles_jarr;
}

cJSON* acv_CircleFitVector2JSON(const vector< acv_CircleFit> &vec, acv_XY center_offset)
{
  cJSON* detectedCircles_jarr = cJSON_CreateArray();
  for(int j=0;j<vec.size();j++)
  {
    cJSON* cfj = cJSON_CreateObject();
    cJSON_AddItemToArray(detectedCircles_jarr, acv_CircleFit2JSON(cfj,vec[j],center_offset) );
  }
  return detectedCircles_jarr;
}

cJSON* acv_AuxPointReport2JSON(const vector< FeatureReport_auxPointReport> &vec, acv_XY center_offset)
{
  
  cJSON* detectedAuxPoint_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON* apj = cJSON_CreateObject();
    cJSON_AddNumberToObject(apj, "status", vec[j].status);
    cJSON_AddNumberToObject(apj, "id", vec[j].def->id);
    cJSON_AddStringToObject(apj, "name", vec[j].def->name);
    
    if(vec[j].status!=FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      cJSON_AddNumberToObject(apj, "x", vec[j].pt.x);
      cJSON_AddNumberToObject(apj, "y", vec[j].pt.y);
    }
    else if(vec[j].na_reason[0] != 0)
    {
      cJSON_AddStringToObject(apj, "na_reason", vec[j].na_reason);
    }
    cJSON_AddItemToArray(detectedAuxPoint_jarr, apj );

  }
  return detectedAuxPoint_jarr;
}
cJSON* acv_SearchPointReport2JSON(const vector< FeatureReport_searchPointReport> &vec, acv_XY center_offset)
{
  
  cJSON* detectedSearchPoint_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON* spj = cJSON_CreateObject();
    cJSON_AddNumberToObject(spj, "status", vec[j].status);
    cJSON_AddNumberToObject(spj, "id", vec[j].def->id);
    cJSON_AddStringToObject(spj, "name", vec[j].def->name);
    
    if(vec[j].status!=FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      cJSON_AddNumberToObject(spj, "x", vec[j].pt.x);
      cJSON_AddNumberToObject(spj, "y", vec[j].pt.y);
    }
    // Why it is NA, when the answer is "the recipe did not say". Only present
    // when there is one -- an NA with no reason is the same disease as a
    // silent substitution: the screen cannot explain what the machine did.
    else if(vec[j].na_reason[0] != 0)
    {
      cJSON_AddStringToObject(spj, "na_reason", vec[j].na_reason);
    }
    AddCalHits2JSON(spj, vec[j].cal_hits, center_offset);
    AddSearchPeaks2JSON(spj, vec[j].cal_peaks);
    cJSON_AddItemToArray(detectedSearchPoint_jarr, spj );

  }
  return detectedSearchPoint_jarr;
}

cJSON* acv_ObjDetectReport2JSON(const vector<FeatureReport_objDetectReport> &vec)
{
  cJSON* arr = cJSON_CreateArray();
  for(int j=0;j<vec.size();j++)
  {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "status", vec[j].status);
    cJSON_AddNumberToObject(o, "id", vec[j].def->id);
    cJSON_AddStringToObject(o, "name", vec[j].def->name);
    if(vec[j].status!=FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      cJSON_AddNumberToObject(o, "bright_mean", vec[j].bright_mean);
      cJSON_AddNumberToObject(o, "bright_max",  vec[j].bright_max);
      cJSON_AddNumberToObject(o, "edge_mean",   vec[j].edge_mean);
      cJSON_AddNumberToObject(o, "edge_max",    vec[j].edge_max);
      // omitted rather than sent as NaN when the region has no dark_thresh --
      // cJSON writes a bare NaN token that JSON.parse rejects outright.
      if(!std::isnan(vec[j].dark_ratio))
      {
        cJSON_AddNumberToObject(o, "dark_ratio",    vec[j].dark_ratio);
        cJSON_AddNumberToObject(o, "dark_area_mm2", vec[j].dark_area_mm2);
      }
    }
    // region corners in OBJECT-FRAME mm (for the WebUI overlay).
    cJSON* corners = cJSON_CreateArray();
    for(int k=0;k<4;k++){
      cJSON* c=cJSON_CreateObject();
      cJSON_AddNumberToObject(c,"x",vec[j].corner[k].x);
      cJSON_AddNumberToObject(c,"y",vec[j].corner[k].y);
      cJSON_AddItemToArray(corners,c);
    }
    cJSON_AddItemToObject(o,"corners",corners);
    cJSON_AddItemToArray(arr, o);
  }
  return arr;
}

cJSON* acv_LineFitVector2JSON(const vector< FeatureReport_lineReport> &vec, acv_XY center_offset)
{

  cJSON* detectedLines_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON* lfj = cJSON_CreateObject();
    cJSON_AddNumberToObject(lfj, "status", vec[j].status);
    cJSON_AddNumberToObject(lfj, "id", vec[j].def->id);
    cJSON_AddStringToObject(lfj, "name", vec[j].def->name);
    if(vec[j].status!=FeatureReport_sig360_circle_line_single::STATUS_NA)
      acv_LineFit2JSON(lfj,vec[j].line,center_offset);
    AddCalHits2JSON(lfj, vec[j].cal_hits, center_offset);
    AddCalProfile2JSON(lfj, vec[j].cal_prof);
    cJSON_AddItemToArray(detectedLines_jarr, lfj );
  }
  return detectedLines_jarr;
}



cJSON* acv_LineFitVector2JSON(const vector< acv_LineFit> &vec, acv_XY center_offset)
{

  cJSON* detectedLines_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON* lfj = cJSON_CreateObject();
    cJSON_AddItemToArray(detectedLines_jarr, acv_LineFit2JSON(lfj,vec[j],center_offset) );
  }
  return detectedLines_jarr;
}

cJSON* acv_Signature2JSON(const vector< acv_XY> &signature)
{

  cJSON* signature_jobj = cJSON_CreateObject();
  cJSON* magnitude_jarr = cJSON_CreateArray();
  cJSON* angle_jarr = cJSON_CreateArray();

  cJSON_AddItemToObject(signature_jobj,"magnitude",magnitude_jarr);
  cJSON_AddItemToObject(signature_jobj,"angle",angle_jarr);
  for(int j=0;j<signature.size();j++)
  {
    cJSON_AddItemToArray(magnitude_jarr, cJSON_CreateNumber(signature[j].x));
    cJSON_AddItemToArray(angle_jarr, cJSON_CreateNumber(signature[j].y));
  }
  return signature_jobj;
}


cJSON* acv_FeatureReport_sig360_circle_line_single2JSON(const FeatureReport_sig360_circle_line_single report )
{
  cJSON* report_jobj = cJSON_CreateObject();
  cJSON_AddNumberToObject(report_jobj, "area", report.area);
  cJSON_AddNumberToObject(report_jobj, "scale", report.scale);
  cJSON_AddStringToObject(report_jobj, "targetName", report.targetName);
  cJSON_AddNumberToObject(report_jobj, "cx", report.Center.x);
  cJSON_AddNumberToObject(report_jobj, "cy", report.Center.y);
  cJSON_AddNumberToObject(report_jobj, "rotate", report.rotate);
  cJSON_AddBoolToObject(report_jobj, "isFlipped", report.isFlipped);
  cJSON_AddNumberToObject(report_jobj, "similarity", report.similarity);
  // Localization trust: fit residual (px), refine points, agreeing inliers, and the
  // tripped gate (empty = ok). Always emitted for a located object. SBM_TRUST_SCORE_DESIGN.md.
  {
    cJSON *tr = cJSON_CreateObject();
    cJSON_AddNumberToObject(tr, "residual", report.trust_residual);
    cJSON_AddNumberToObject(tr, "npts", report.trust_npts);
    cJSON_AddNumberToObject(tr, "inliers", report.trust_ninliers);
    if (report.trust_code[0]) cJSON_AddStringToObject(tr, "code", report.trust_code);
    cJSON_AddItemToObject(report_jobj, "trust", tr);
  }

  acv_XY offset = {0};
  const vector<FeatureReport_circleReport> &detectedCircle = *report.detectedCircles;
  cJSON_AddItemToObject(report_jobj,"detectedCircles",
    acv_CircleFitVector2JSON(detectedCircle,offset));

  const vector<FeatureReport_lineReport> &detectedLines =*report.detectedLines;
  cJSON_AddItemToObject(report_jobj,"detectedLines",
    acv_LineFitVector2JSON(detectedLines,offset));

  const vector<FeatureReport_auxPointReport> &detectedAuxPoints =*report.detectedAuxPoints;
  cJSON_AddItemToObject(report_jobj,"auxPoints",
    acv_AuxPointReport2JSON(detectedAuxPoints,offset));

  const vector<FeatureReport_searchPointReport> &detectedSearchPoints =*report.detectedSearchPoints;
  cJSON_AddItemToObject(report_jobj,"searchPoints",
    acv_SearchPointReport2JSON(detectedSearchPoints,offset));

  cJSON_AddItemToObject(report_jobj,"objDetects",
    acv_ObjDetectReport2JSON(*report.detectedObjDetects));

  const vector< FeatureReport_judgeReport> &judgeReports=*report.judgeReports;
  cJSON_AddItemToObject(report_jobj,"judgeReports",
    JudgeReportVector2JSON(judgeReports,offset));



  return report_jobj;
}

int cameraCalib2JSON(cJSON* jobj,FeatureManager_BacPac *bacpac)
{
  if(jobj==NULL||bacpac==NULL || bacpac->sampler==NULL)
  {
  
    return -1;
  }
  {
    cJSON_AddNumberToObject(jobj, "ppb2b", 1);
    cJSON_AddNumberToObject(jobj, "mmpb2b", bacpac->sampler->mmpP_ideal());
    // back_light_target was a stageLightInfo field; consumers can now read
    // fieldCal.stats.bright_mean from data/field_calib.json instead.
    if(bacpac->cam!=NULL)
    {
      float expTime;
      if (CameraLayer::ACK == bacpac->cam->GetExposureTime(&expTime))
      {
        cJSON_AddNumberToObject(jobj, "exposure_time", expTime);
      }
    }

  }
  return 0;
}


cJSON* genXYObject(acv_XY xy)
{
  
  cJSON* jxy = cJSON_CreateObject();
  cJSON_AddNumberToObject(jxy, "x", xy.x);
  cJSON_AddNumberToObject(jxy, "y", xy.y);
  return jxy;
}


cJSON* MatchingReport2JSON(const FeatureReport *report )
{

  if(report==NULL)
  {
    return NULL;
  }

  if(report->type == FeatureReport::cjson)
  {
    cJSON* tmp_jobj=cJSON_Duplicate(report->data.cjson_report.cjson,true);
    if(tmp_jobj==NULL)
      tmp_jobj=cJSON_CreateObject();
    return tmp_jobj;
    
  }
  if(report->type == FeatureReport::custom)
  {
    cJSON* tmp_jobj=cJSON_Duplicate(report->data.custom_report.cjson,true);
    if(tmp_jobj==NULL)
      tmp_jobj=cJSON_CreateObject();
    return tmp_jobj;
    
  }
  cJSON* report_jobj = cJSON_CreateObject();
  //for(int i=0;i<featureBundle.size();i++)


  switch (report->type)
  {
    case FeatureReport::binary_processing_group:
    {
      cJSON_AddStringToObject(report_jobj, "type", FeatureManager_binary_processing_group::GetFeatureTypeName());

      /*
      cJSON* label_jarr = cJSON_CreateArray();
      cJSON_AddItemToObject(report_jobj,"labeledData",label_jarr);

      const vector<acv_LabeledData> *ldata =
      report->data.binary_processing_group.labeledData;


      for(int j=2;j<ldata->size();j++)
      {
        if((*ldata)[j].area<2)continue;
        cJSON* label_jobj = cJSON_CreateObject();
        cJSON_AddItemToArray(label_jarr, label_jobj );
        cJSON_AddNumberToObject(label_jobj, "area", (*ldata)[j].area);
      }*/

      cJSON_AddNumberToObject(report_jobj, "error", report->data.binary_processing_group.error);

      
      cJSON_AddNumberToObject(report_jobj, "mmpp", report->data.binary_processing_group.mmpp);


      cJSON_AddStringToObject(report_jobj, "subFeatureDefSha1", report->data.binary_processing_group.subFeatureDefSha1);
      cJSON* reports_jarr = cJSON_CreateArray();
      cJSON_AddItemToObject(report_jobj,"reports",reports_jarr);


      const vector<const FeatureReport*> *sub_reports =
      report->data.binary_processing_group.reports;

      for(int j=0;j<sub_reports->size();j++)
      {
        cJSON_AddItemToArray(reports_jarr,
          MatchingReport2JSON((*sub_reports)[j]));
      }
    }
    break;

    case FeatureReport::sig360_extractor:
    {

      cJSON_AddNumberToObject(report_jobj, "error", report->data.sig360_extractor.error);
      cJSON_AddNumberToObject(report_jobj, "area", report->data.sig360_extractor.area);
      cJSON_AddNumberToObject(report_jobj, "mmpp", report->data.sig360_extractor.mmpp);
      cJSON_AddNumberToObject(report_jobj, "cx", report->data.sig360_extractor.Center.x);
      cJSON_AddNumberToObject(report_jobj, "cy", report->data.sig360_extractor.Center.y);

      {
        cJSON* cam_param = cJSON_CreateObject();
        cJSON_AddItemToObject(report_jobj, "cam_param", cam_param);
        cameraCalib2JSON(cam_param,report->bacpac);
      }
      
      const vector<acv_CircleFit> *detectedCircle =
      report->data.sig360_extractor.detectedCircles;
      cJSON_AddStringToObject(report_jobj, "type", FeatureManager_sig360_extractor::GetFeatureTypeName());

      cJSON_AddItemToObject(report_jobj,"detectedCircles",
        acv_CircleFitVector2JSON(*detectedCircle,report->data.sig360_extractor.Center));

      const vector<acv_LineFit> *detectedLines =
      report->data.sig360_extractor.detectedLines;

      cJSON_AddItemToObject(report_jobj,"detectedLines",
        acv_LineFitVector2JSON(*detectedLines,report->data.sig360_extractor.Center));

      cJSON_AddItemToObject(report_jobj, "signature", acv_Signature2JSON(*(report->data.sig360_extractor.signature)));
    }
    break;

    case FeatureReport::sig360_circle_line:
    {
      cJSON_AddStringToObject(report_jobj, "type", FeatureManager_sig360_circle_line::GetFeatureTypeName());

      cJSON_AddStringToObject(report_jobj, "ver", "0.0.0.0");

      vector<FeatureReport_sig360_circle_line_single> &scl_reports =
        *report->data.sig360_circle_line.reports;

      // Say so when the working region is why the object list is short or
      // empty. Otherwise "the region rejected everything" and "nothing was
      // found" are the same reply.
      cJSON_AddNumberToObject(report_jobj, "region_dropped",
                              report->data.sig360_circle_line.region_dropped);

      // ALWAYS emitted, unlike `locate` below. `locate` is a complaint and is
      // absent on a good run; this is a fact about every run, and the whole
      // point of it is to be readable when nothing went wrong.
      cJSON_AddStringToObject(report_jobj, "locator",
          report->data.sig360_circle_line.locator_used == 1 ? "shape_based" : "sig360");

      // The locate outcome, and ONLY when it has something to say -- a
      // successful run adds nothing here, so existing defs hash and diff the
      // same and the key's presence itself means "the locator has a comment".
      {
        const auto &L = report->data.sig360_circle_line.locate;
        if (L.reason[0] != 0)
        {
          cJSON *loc = cJSON_CreateObject();
          cJSON_AddItemToObject(report_jobj, "locate", loc);
          cJSON_AddStringToObject(loc, "reason", L.reason);

          if (L.code[0]) cJSON_AddStringToObject(loc, "code", L.code);
          cJSON_AddNumberToObject(loc, "candidates", L.candidates);
          // best/thres only when a score was actually computed. Emitting NaN
          // would serialise as `null` and read on screen as "scored zero",
          // which is a different and much worse claim than "never scored".
          if (L.best == L.best)  cJSON_AddNumberToObject(loc, "best",  L.best);
          if (L.thres == L.thres) cJSON_AddNumberToObject(loc, "thres", L.thres);
        }
      }

      if(report->bacpac!=NULL)
      {
        cJSON* cam_param = cJSON_CreateObject();
        cJSON_AddItemToObject(report_jobj, "cam_param", cam_param);
        cameraCalib2JSON(cam_param,report->bacpac);
      }
      cJSON_AddNumberToObject(report_jobj, "error", report->data.sig360_circle_line.error);
      cJSON* reports_jarr = cJSON_CreateArray();
      cJSON_AddItemToObject(report_jobj,"reports",reports_jarr);
      for(int i=0;i<scl_reports.size();i++)
      {

        cJSON_AddItemToArray(reports_jarr,
            acv_FeatureReport_sig360_circle_line_single2JSON(scl_reports[i]));

      }

    }
    break;
    // case FeatureReport::camera_calibration:
    // {
    //   cJSON_AddStringToObject(report_jobj, "type", FM_camera_calibration::GetFeatureTypeName());

    //   cJSON_AddStringToObject(report_jobj, "ver", "0.0.0.0");
    //   cJSON_AddNumberToObject(report_jobj, "error", report->data.camera_calibration.error);
    //   if(report->data.camera_calibration.error==FeatureReport_ERROR::NONE)
    //   {
    //     cameraCalib2JSON(report_jobj,report->data.camera_calibration.sampler);
    //   }

    // }
    // break;    
    case FeatureReport::stage_light_report:
    {
      cJSON_AddStringToObject(report_jobj, "type", FeatureManager_stage_light_report::GetFeatureTypeName());

      cJSON_AddStringToObject(report_jobj, "ver", "0.0.0.0");

      {
        cJSON* cam_param = cJSON_CreateObject();
        cJSON_AddItemToObject(report_jobj, "cam_param", cam_param);
        cameraCalib2JSON(cam_param,report->bacpac);
      }
      
      const FeatureReport_stage_light_report &rep=report->data.stage_light_report;

      {
        acv_XY xy={(float)rep.targetImageDim[0], (float)rep.targetImageDim[1]};
        cJSON_AddItemToObject(report_jobj,"target_image_dim",genXYObject(xy));
      }


      {
        vector<stage_light_grid_node_info> &gridInfo=*rep.gridInfo;

        

        cJSON* jgridInfo = cJSON_CreateArray();

        for(int j=0;j<gridInfo.size();j++)
        {
          
          cJSON* nodeInfo = cJSON_CreateObject();
          
          cJSON_AddItemToObject(nodeInfo,"location",genXYObject(gridInfo[j].nodeLocation));
          cJSON_AddItemToObject(nodeInfo,"index",genXYObject(gridInfo[j].nodeIndex));
          
          cJSON_AddNumberToObject(nodeInfo, "sigma",gridInfo[j].backLightSigma);
          cJSON_AddNumberToObject(nodeInfo, "mean",gridInfo[j].backLightMean);
          cJSON_AddNumberToObject(nodeInfo, "error",gridInfo[j].error);
          cJSON_AddNumberToObject(nodeInfo, "samp_rate",gridInfo[j].sampRate);
          cJSON_AddItemToArray(jgridInfo, nodeInfo );
        }

        cJSON_AddNumberToObject(report_jobj, "error", 0);
        cJSON_AddItemToObject(report_jobj,"grid_info",jgridInfo);
      
      }


    }
    break;
    default:
      LOGE("UNKNOWN type:%d",report->type);
  }

  return report_jobj;
}