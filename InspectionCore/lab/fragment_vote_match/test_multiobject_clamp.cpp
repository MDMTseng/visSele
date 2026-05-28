// Hard test: 3 objects at random (pos,rot), ONE with a connected clamp, all with
// occlusion (missing arcs) + point noise. Fragment-vote should recover each pose
// and ignore the clamp.
#include "FragmentVoteMatch.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
using namespace std;
static float Rprofile(float phi){ return 70+22*cosf(phi)+12*cosf(2*phi+0.7f)+8*sinf(3*phi); }
// closed template contour (object frame, centered)
static vector<acv_XY> templateContour(){ vector<acv_XY> c; for(int i=0;i<720;i++){float phi=2*M_PI*i/720; float r=Rprofile(phi); c.push_back({r*cosf(phi),r*sinf(phi)});} return c; }

int main(){
  FVParams P; P.ds=4; P.residual_thresh=0.05f; P.min_inliers=2; P.pos_tol=15; P.ang_tol=0.08f; P.peak_ratio=0.6f; P.max_cands_per_frag=6; P.min_inliers=3;
  FVTemplate tmpl=fv_build_template(templateContour(),P);

  mt19937 rng(123); normal_distribution<float> nz(0,0.4f);
  struct GT{float x,y,rot;}; vector<GT> gts={{300,300,20*(float)M_PI/180},{700,350,-50*(float)M_PI/180},{500,650,130*(float)M_PI/180}};
  vector<vector<acv_XY>> frags;

  for(size_t o=0;o<gts.size();o++){
    GT g=gts[o]; float ct=cosf(g.rot),st=sinf(g.rot);
    // realistic edge tracing: boundary broken into many fragments (~90 pts) with
    // ~14-pt gaps, PLUS a fully-occluded arc dropped entirely.
    int occA= (o==0)?40:(o==1)?300:600; int occLen=120; // occluded arc (object hidden here)
    vector<acv_XY> cur; int since=0;
    for(int i=0;i<720;i++){
      bool occ = (i>=occA && i<occA+occLen);
      bool gap = (i%160)>=144; // moderate fragments
      if(occ||gap){ if(cur.size()>10) frags.push_back(cur); cur.clear(); continue; }
      float phi=2*M_PI*i/720; float r=Rprofile(phi);
      acv_XY v={r*cosf(phi),r*sinf(phi)};
      acv_XY P_={ g.x+ct*v.X-st*v.Y+nz(rng), g.y+st*v.X+ct*v.Y+nz(rng) };
      cur.push_back(P_);
    }
    if(cur.size()>10) frags.push_back(cur);
    // object 0: add a CLAMP (straight bar edges) connected near its boundary
    if(o==0){
      vector<acv_XY> e1,e2;
      for(int k=0;k<60;k++){ e1.push_back({g.x+50+k*3.0f, g.y-15+nz(rng)}); e2.push_back({g.x+50+k*3.0f, g.y+15+nz(rng)}); }
      frags.push_back(e1); frags.push_back(e2);
    }
  }
  printf("scene: %zu fragments from 3 objects (occlusion + noise) + 1 clamp(2 straight edges)\n",frags.size());

  auto poses=fv_match(tmpl,frags,P);
  printf("found %zu object instances:\n",poses.size());
  for(auto&ps:poses) printf("  pose (%.1f,%.1f) rot=%+6.1f deg  score=%.0f inliers=%d\n",ps.x,ps.y,ps.theta*180/M_PI,ps.score,ps.inliers);

  // match to GT
  int ok=0;
  for(auto&g:gts){ bool found=false; for(auto&ps:poses){ if(hypotf(ps.x-g.x,ps.y-g.y)<12 && fabsf(atan2f(sinf(ps.theta-g.rot),cosf(ps.theta-g.rot)))<3*M_PI/180) found=true; } if(found)ok++; printf("  GT (%.0f,%.0f,%+.0f) -> %s\n",g.x,g.y,g.rot*180/M_PI, found?"MATCHED":"MISSED"); }
  printf("%s (%d/3 objects recovered, clamp not a false positive=%s)\n", ok==3?"PASS":"FAIL", ok, poses.size()<=3?"yes":"NO-extra-peaks");
  return 0;
}
