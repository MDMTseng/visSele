#ifndef ME_PHASE_H
#define ME_PHASE_H

// Per-frame phase timers and work counters, shared by every layer of the
// inspection.
//
// Timing alone answers "how long did it take", which is only useful for the
// machine it was measured on, with the def it was measured with. The question
// that actually gets asked is the other one: what happens to this number when
// the part is twice the size, when a primitive gains calipers, when the edge
// estimator stops being a parabola on three samples and starts being a profile
// comparison over the whole window. That needs the WORK as well as the time --
// scans, samples -- so a cost per unit of work can be divided out and applied
// to a case nobody has built yet.
//
// Deliberately free of any dependency: Caliper sits below the matching engine
// and must not include it just to say how many samples it took.
//
// Slots are claimed BY NAME on first use and keep their index for the life of
// the process, so a measurement can be added where the work is without the
// producer and the reporter agreeing on an enum first. The threading
// assumption is the engine's: one inspection at a time, which the engine lock
// enforces.

namespace mephase {

static const int PHASE_MAX      = 12;
static const int PHASE_NAME_MAX = 20;

// Timers (ms, accumulated over the frame).
int    slot(const char *name);        // claim/find; -1 when full
void   add(int slot, double ms);
double nowMs();
void   reset();                       // accumulators only; names stay claimed
extern char   name_[PHASE_MAX][PHASE_NAME_MAX];
extern double ms_[PHASE_MAX];
extern int    n_;

// Counters (dimensionless work per frame), same slot discipline, separate space.
//
// ATTRIBUTED TO THE PHASE THAT IS RUNNING. A caliper scan does not know whether
// it was called to re-locate a morph anchor or to take the official
// measurement, and the two are timed separately -- so counting them into one
// pot and dividing a single phase's time by it produces a cost per sample that
// is simply wrong, in the flattering direction. The counter's key is
// "<phase>/<name>", so every ratio has a matching numerator and denominator.
static const int COUNT_MAX = 24;
static const int PHASE_STACK_MAX = 8;
int  countSlot(const char *name);        // takes the FULL key, no prefixing
void countAdd(int slot, double v);
const char *currentPhase();              // innermost running phase, or ""
void pushPhase(int slot);
void popPhase();
extern char   cname_[COUNT_MAX][PHASE_NAME_MAX];
extern double cval_[COUNT_MAX];
extern int    cn_;

// Scoped timer. Declare one at the top of the region to be measured:
//   { ME_PHASE("sbm"); ms = shapeMatcher->match(scene); }
class Timer {
  int    slot_;
  double t0_;
public:
  explicit Timer(const char *n) : slot_(slot(n)), t0_(slot_ >= 0 ? nowMs() : 0.0)
  { if (slot_ >= 0) pushPhase(slot_); }
  // Ends the phase early, for a region that stops before its enclosing scope
  // does. Idempotent, and the destructor goes through it -- so a stopped timer
  // cannot be charged twice on the way out.
  void stop()
  { if (slot_ < 0) return; add(slot_, nowMs() - t0_); popPhase(); slot_ = -1; }
  ~Timer() { stop(); }
};

// Counts `v` units of work against the phase currently running. Cheap enough
// for once per caliper scan; not for once per pixel.
void count(const char *n, double v);

} // namespace mephase

#define ME_PHASE_CAT2(a, b) a##b
#define ME_PHASE_CAT(a, b) ME_PHASE_CAT2(a, b)
#define ME_PHASE(name) mephase::Timer ME_PHASE_CAT(_me_phase_, __LINE__)(name)

#endif
