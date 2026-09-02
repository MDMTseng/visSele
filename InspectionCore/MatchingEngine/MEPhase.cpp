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

// COMPARE WHAT WILL BE STORED, NOT WHAT WAS PASSED.
//
// The names are kept in fixed buffers, so a long one is truncated on the way
// in. Comparing the full string against the truncated copy means such a name
// never matches itself: every call claimed a FRESH slot until the table was
// full, and after that the measurement silently vanished. Found on
// "measure/spcv_samp_min" -- 21 characters against a 20-byte buffer -- which
// reported an average of 5.3 for a quantity that is ~5800 every frame.
//
// Truncating the lookup key the same way makes it idempotent at any length.
static void fit(char *dst, const char *src)
{
  snprintf(dst, PHASE_NAME_MAX, "%s", src);
}

int slot(const char *n)
{
  if (n == 0) return -1;
  char key[PHASE_NAME_MAX];
  fit(key, n);
  for (int i = 0; i < n_; i++)
    if (strcmp(name_[i], key) == 0) return i;
  if (n_ >= PHASE_MAX) return -1;
  snprintf(name_[n_], PHASE_NAME_MAX, "%s", key);
  ms_[n_] = 0.0;
  return n_++;
}

void add(int s, double ms) { if (s >= 0 && s < PHASE_MAX) ms_[s] += ms; }

int countSlot(const char *n)
{
  if (n == 0) return -1;
  char key[PHASE_NAME_MAX];
  fit(key, n);          // see slot(): the stored form is what must be compared
  for (int i = 0; i < cn_; i++)
    if (strcmp(cname_[i], key) == 0) return i;
  if (cn_ >= COUNT_MAX) return -1;
  snprintf(cname_[cn_], PHASE_NAME_MAX, "%s", key);
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
  char key[PHASE_NAME_MAX * 2];   // countSlot truncates; this only has to hold it
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
