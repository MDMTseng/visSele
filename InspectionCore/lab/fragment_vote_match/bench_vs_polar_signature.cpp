#include "EdgeSignature.h"
#include "FragmentVoteMatch.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <chrono>
#include <random>
using namespace std;
using Clk=chrono::high_resolution_clock;
static float Rprofile(float phi){ return 70+22*cosf(phi)+12*cosf(2*phi+0.7f)+8*sinf(3*phi); }

int main(){
  mt19937 rng(1); normal_distribution<float> nz(0,0.3f);

  // ---- Polar edge-signature: two 360-bin signatures, match (1D correlation) ----
  vector<float> rr(360),rw(360),cr(360),cw(360);
  for(int i=0;i<360;i++){ float phi=2*M_PI*i/360; rr[i]=Rprofile(phi); rw[i]=20; cr[i]=Rprofile(phi-0.5f)+nz(rng); cw[i]=20; }
  int NP=20000;
  auto t0=Clk::now();
  volatile float acc=0;
  for(int it=0;it<NP;it++){ auto m=edge_signature_match(rr,rw,cr,cw,180,true); acc+=m.angle_rad; }
  auto t1=Clk::now();
  double polar_us=chrono::duration<double,micro>(t1-t0).count()/NP;

  // ---- Fragment-vote: template + ~6 fragments of one object, full fv_match ----
  vector<acv_XY> contour; for(int i=0;i<720;i++){float phi=2*M_PI*i/720;float r=Rprofile(phi);contour.push_back({r*cosf(phi),r*sinf(phi)});}
  FVParams P; P.ds=4; P.residual_thresh=0.05f; P.min_inliers=2;
  FVTemplate tmpl=fv_build_template(contour,P);
  // build ~6 fragments (one object at 0,0 rot 0)
  vector<vector<acv_XY>> frags; vector<acv_XY> cur;
  for(int i=0;i<720;i++){ if((i%120)>=104){ if(cur.size()>10)frags.push_back(cur); cur.clear(); continue;} float phi=2*M_PI*i/720;float r=Rprofile(phi); cur.push_back({r*cosf(phi)+nz(rng),r*sinf(phi)+nz(rng)});}
  if(cur.size()>10)frags.push_back(cur);
  int NF=2000;
  auto t2=Clk::now();
  volatile int acc2=0;
  for(int it=0;it<NF;it++){ auto ps=fv_match(tmpl,frags,P); acc2+=ps.size(); }
  auto t3=Clk::now();
  double frag_us=chrono::duration<double,micro>(t3-t2).count()/NF;

  printf("polar edge-signature match (360 bins, +/-180 search, flip): %.1f us/object\n",polar_us);
  printf("fragment-vote full match (%zu fragments -> poses):          %.1f us/scene\n",frags.size(),frag_us);
  printf("ratio: fragment-vote is %.1fx the polar-signature match cost\n",frag_us/polar_us);
  return 0;
}
