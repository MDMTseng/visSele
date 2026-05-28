// Prototype: contour-fragment turning-function matching + voting.
// Template = object boundary turning function. Scene = object rotated +30deg
// with a connected CLAMP (straight edges). Each fragment matched to template
// -> (rotation, residual); vote on rotation. Clamp fragments should be rejected
// by residual, object fragments should agree on +30deg.
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>
using namespace std;
static const int M=360;
static float Rprofile(float phi){ return 70+22*cosf(phi)+12*cosf(2*phi+0.7f)+8*sinf(3*phi); }
static float angdiff(float a){ return atan2f(sinf(a),cosf(a)); }

// template turning function: tangent angle per boundary point (object frame)
static void buildTemplate(vector<float>&tau){
  vector<float> px(M),py(M);
  for(int i=0;i<M;i++){ float phi=2*M_PI*i/M; float r=Rprofile(phi); px[i]=r*cosf(phi); py[i]=r*sinf(phi);}
  tau.resize(M);
  for(int i=0;i<M;i++){ int a=(i-1+M)%M,b=(i+1)%M; tau[i]=atan2f(py[b]-py[a],px[b]-px[a]); }
}

// match a fragment turning function against template -> best rotation + residual
static void matchFrag(const vector<float>&tf,const vector<float>&tau,float&bestRot,float&bestErr){
  int L=tf.size(); bestErr=1e30; bestRot=0;
  for(int s=0;s<M;s++){
    float ss=0,cc=0;
    for(int k=0;k<L;k++){ float d=tf[k]-tau[(s+k)%M]; ss+=sinf(d); cc+=cosf(d);}
    float dth=atan2f(ss,cc);
    float err=0; for(int k=0;k<L;k++){ float e=angdiff(tf[k]-tau[(s+k)%M]-dth); err+=e*e;}
    err/=L;
    if(err<bestErr){bestErr=err;bestRot=dth;}
  }
}

int main(){
  vector<float> tau; buildTemplate(tau);
  float inj=30*M_PI/180;
  mt19937 rng(7); normal_distribution<float> nz(0,0.03f); // tangent noise (rad)

  vector<vector<float>> frags; vector<const char*> kind;
  // 5 OBJECT fragments (rotated +inj), skipping a 50-deg sector occluded by clamp
  int occ_lo=350, occ_hi=410; // occluded arc indices (mod M) ~ where clamp attaches
  int starts[5]={20,90,160,230,300}; int len=45;
  for(int f=0;f<5;f++){
    vector<float> tf;
    bool occluded=false;
    for(int k=0;k<len;k++){ int idx=(starts[f]+k)%M; int idxw=idx<occ_lo?idx+M:idx; if(idxw>=occ_lo&&idxw<=occ_hi)occluded=true; tf.push_back(tau[idx]+inj+nz(rng)); }
    if(occluded) continue; // this fragment is where clamp covers -> not present
    frags.push_back(tf); kind.push_back("object");
  }
  // 3 CLAMP fragments: straight edges (constant tangent angle) of the gripper bar
  for(float cang : {0.2f, 0.2f+(float)M_PI, 1.4f}){
    vector<float> tf; for(int k=0;k<40;k++) tf.push_back(cang+nz(rng)); frags.push_back(tf); kind.push_back("CLAMP");
  }

  printf("fragment matches:\n");
  vector<float> votes;
  float errThresh=0.02f;
  for(size_t f=0;f<frags.size();f++){
    float rot,err; matchFrag(frags[f],tau,rot,err);
    bool acc=err<errThresh;
    printf("  [%-6s] rot=%+6.1f deg  residual=%.4f  -> %s\n",kind[f],rot*180/M_PI,err, acc?"VOTE":"reject");
    if(acc) votes.push_back(rot);
  }
  // vote: CONSENSUS PEAK (not mean). Object fragments cluster; clamp scatters.
  float tol=3*M_PI/180; int bestCnt=0; float bestCenter=0;
  for(size_t i=0;i<votes.size();i++){
    int cnt=0; float ss2=0,cc2=0;
    for(size_t j=0;j<votes.size();j++){ if(fabsf(angdiff(votes[j]-votes[i]))<tol){cnt++; ss2+=sinf(votes[j]); cc2+=cosf(votes[j]);} }
    if(cnt>bestCnt){bestCnt=cnt; bestCenter=atan2f(ss2,cc2);}
  }
  printf("\nconsensus peak = %.2f deg from %d/%zu agreeing fragments (injected 30.00)  err=%.2f deg\n",
         bestCenter*180/M_PI, bestCnt, votes.size(), fabsf(angdiff(bestCenter-inj))*180/M_PI);
  printf("%s\n", fabsf(angdiff(bestCenter-inj))*180/M_PI<2 ? "PASS (clamp scattered & ignored, object pose recovered)":"FAIL");
  return 0;
}
