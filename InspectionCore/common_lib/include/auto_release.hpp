#ifndef AUTO_RELEASE_HPP
#define AUTO_RELEASE_HPP

#include <cstdlib>
#include <cJSON.h>

// Ownership for the two resource kinds this codebase leaks: cJSON trees and
// malloc'd buffers (cJSON_Print, ReadText). Both were being freed by a single
// call at the bottom of a long block, which is exactly the line that every
// `break`, `return` and thrown exception skips -- and the BPG handlers are
// hundreds of lines with 20+ early exits each. A stack guard is the only form
// that survives all three.
//
// Deliberately not unique_ptr: these are C allocations reached through C APIs,
// and a named type reads better at the ~40 call sites than a unique_ptr with a
// custom deleter typedef.

// Owns a cJSON tree until end of scope.
class CJsonHold
{
public:
  explicit CJsonHold(cJSON *p = NULL) : ptr(p) {}
  ~CJsonHold() { if (ptr) cJSON_Delete(ptr); }

  // Hand ownership back to the caller (for the paths that return the tree).
  cJSON *release() { cJSON *p = ptr; ptr = NULL; return p; }
  // Adopt a new tree, freeing whatever was held before.
  void reset(cJSON *p = NULL) { if (ptr && ptr != p) cJSON_Delete(ptr); ptr = p; }

  cJSON *get() const { return ptr; }
  operator cJSON *() const { return ptr; }
  cJSON *operator->() const { return ptr; }

private:
  cJSON *ptr;
  CJsonHold(const CJsonHold &);
  CJsonHold &operator=(const CJsonHold &);
};

// Owns a malloc'd buffer until end of scope. Note this frees with free(), which
// is what these allocations actually need -- several call sites were releasing
// cJSON_Print / ReadText results with `delete`, which is undefined behaviour
// that only happens to work while operator delete forwards to free.
class MallocHold
{
public:
  explicit MallocHold(void *p = NULL) : ptr(p) {}
  ~MallocHold() { if (ptr) free(ptr); }

  void *release() { void *p = ptr; ptr = NULL; return p; }
  void reset(void *p = NULL) { if (ptr && ptr != p) free(ptr); ptr = p; }

  void *get() const { return ptr; }
  char *str() const { return (char *)ptr; }

private:
  void *ptr;
  MallocHold(const MallocHold &);
  MallocHold &operator=(const MallocHold &);
};

#endif
