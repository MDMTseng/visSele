#include "MEPhase.h"
#include <cstring>
#include <cstdio>
#include <ctime>

namespace mephase {

char   name_[PHASE_MAX][PHASE_NAME_MAX] = {{0}};
double ms_[PHASE_MAX] = {0};
int    n_ = 0;

char   cname_[COUNT_MAX][PHASE_NAME_MAX] = {{0}};
double cval_[COUNT_MAX] = {0};
int    cn_ = 0;

double nowMs()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int slot(const char *n)
{
  if (n == 0) return -1;
  for (int i = 0; i < n_; i++)
    if (strncmp(name_[i], n, PHASE_NAME_MAX) == 0) return i;
  if (n_ >= PHASE_MAX) return -1;
  snprintf(name_[n_], PHASE_NAME_MAX, "%s", n);
  ms_[n_] = 0.0;
  return n_++;
}

void add(int s, double ms) { if (s >= 0 && s < PHASE_MAX) ms_[s] += ms; }

int countSlot(const char *n)
{
  if (n == 0) return -1;
  for (int i = 0; i < cn_; i++)
    if (strncmp(cname_[i], n, PHASE_NAME_MAX) == 0) return i;
  if (cn_ >= COUNT_MAX) return -1;
  snprintf(cname_[cn_], PHASE_NAME_MAX, "%s", n);
  cval_[cn_] = 0.0;
  return cn_++;
}

void countAdd(int s, double v) { if (s >= 0 && s < COUNT_MAX) cval_[s] += v; }

// Which phase is running, innermost first. A depth of 8 is far more than the
// nesting the engine has; overflowing it drops the attribution rather than
// corrupting the stack, because a mis-attributed number is worse than none.
static int   pstack_[PHASE_STACK_MAX];
static int   pdepth_ = 0;

void pushPhase(int s) { if (pdepth_ < PHASE_STACK_MAX) pstack_[pdepth_] = s; pdepth_++; }
void popPhase()       { if (pdepth_ > 0) pdepth_--; }
const char *currentPhase()
{
  if (pdepth_ <= 0 || pdepth_ > PHASE_STACK_MAX) return "";   // overflowed: no attribution
  int s = pstack_[pdepth_ - 1];
  return (s >= 0 && s < n_) ? name_[s] : "";
}

void count(const char *n, double v)
{
  if (n == 0) return;
  const char *ph = currentPhase();
  char key[PHASE_NAME_MAX * 2];
  if (ph[0]) snprintf(key, sizeof(key), "%s/%s", ph, n);
  else       snprintf(key, sizeof(key), "%s", n);
  countAdd(countSlot(key), v);
}

// Only the accumulators reset. The NAMES stay claimed for the life of the
// process, so slot k means the same thing in every frame -- a reader polling
// once a second is not made to re-learn the mapping, and a phase some frames
// never enter reports 0 rather than vanishing and shifting the rest up.
void reset()
{
  for (int i = 0; i < n_;  i++) ms_[i]   = 0.0;
  for (int i = 0; i < cn_; i++) cval_[i] = 0.0;
  // A timer that was still running at frame end (an early return past a
  // stop()) would otherwise leak a stack entry every frame and mis-attribute
  // everything after it.
  pdepth_ = 0;
}

} // namespace mephase
