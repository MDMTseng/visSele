// latency_stages -- build the core's timing behaviour up one layer at a time
// and find the layer where the jitter stops being acceptable.
//
// The expectation for this pipeline is +/-10ms of jitter. What the core
// actually shows, measured 2026-08-10 over 65,144 frames with every earlier
// defect fixed:
//
//   e2e       avg 12.07ms   max 164.3ms
//   insp_off  avg  3.69ms   max 119.2ms     <- wall time the inspection thread
//                                              spent NOT RUNNING, per frame
//
// 3.69ms of every 11.3ms stage, and a 119ms worst case, on a host at 15-75%
// non-core load. That is an order of magnitude worse than it should be, and
// measuring it inside 200k lines cannot say which layer is responsible --
// every stage there runs on top of every other one.
//
// So: start from nothing and add ONE thing per stage, with the same histogram
// and the same bucket edges as wiringPanel.cpp, so a number here can be put
// straight next to a number from the core.
//
//   0  timer only          a thread that wakes on a 2ms schedule. The floor:
//                          whatever this shows, nothing above it can beat.
//   1  + handoff           producer -> queue -> consumer, polled. Adds the
//                          report path's shape and nothing else.
//   2  + allocation        build and free a ~553-node cJSON tree per frame,
//                          which is what FeatureReport2Json does. Prime
//                          suspect for insp_off: malloc blocks, and blocking
//                          burns no CPU, so it looks exactly like this.
//   3  + compute           an 11.6ms spin, sized to the measured inspect avg.
//   4  + second CPU thread a preview-sized compressor running alongside,
//                          contending for both cores and the allocator.
//   5  + serial write      a real blocking write() to a pty, paced like the
//                          real link.
//
// Each stage is strictly stage N-1 plus one addition, so the stage where the
// tail jumps IS the answer. Run them in order and compare `off` (wall minus
// this thread's own cpu) -- the same instrument that found the problem in the
// core.
//
//   ./latency_stages --stage 2 --seconds 60
//
// Deliberately does NOT link the camera or the matching engine. Those are
// stages 6 and 7, and there is no point adding them until the cheap layers are
// exonerated -- if stage 2 already shows 100ms, the camera is irrelevant.

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#if defined(__APPLE__)
#include <util.h>       // openpty
#elif !defined(_WIN32)
#include <pty.h>
#endif
#ifdef __APPLE__
#include <pthread.h>
#include <sys/qos.h>
#endif
#include "cJSON.h"

// Identical to PERIF_HIST_EDGES_MS in wiringPanel.cpp. Not "similar" --
// identical, because the whole point is cross-reading the two.
static const double EDGES_MS[] = {
  0.5, 1, 2, 5, 10, 20, 50, 100, 200, 300, 400, 600, 800, 1200, 2000
};
static const int NB = (int)(sizeof(EDGES_MS) / sizeof(EDGES_MS[0])) + 1;

struct Hist {
  const char *name;
  unsigned long long bucket[16] = {0};
  unsigned long long n = 0;
  double sum = 0, max = 0;
  explicit Hist(const char *nm) : name(nm) {}
  void add(double ms)
  {
    n++; sum += ms;
    if (ms > max) max = ms;
    int i = 0;
    while (i < NB - 1 && ms >= EDGES_MS[i]) i++;
    bucket[i]++;
  }
  void print() const
  {
    printf("%-9s n=%-8llu avg=%7.3f max=%9.2f |", name, n,
           n ? sum / n : 0.0, max);
    for (int i = 0; i < NB; i++)
    {
      if (!bucket[i]) continue;
      if (i == 0)            printf(" <%gms:%llu", EDGES_MS[0], bucket[i]);
      else if (i >= NB - 1)  printf(" >=%gms:%llu", EDGES_MS[NB - 2], bucket[i]);
      else                   printf(" %g-%gms:%llu", EDGES_MS[i - 1], EDGES_MS[i], bucket[i]);
    }
    printf("\n");
  }
};

static inline uint64_t now_us()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}
static inline uint64_t cpu_us()
{
  struct timespec ts;
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0)
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
  return 0;
}
static void nap_us(uint64_t us)
{
  struct timespec ts;
  ts.tv_sec  = (time_t)(us / 1000000ull);
  ts.tv_nsec = (long)((us % 1000000ull) * 1000ull);
  nanosleep(&ts, NULL);
}
static void raise_qos()
{
#ifdef __APPLE__
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

// The allocation load, sized from the real thing: FeatureReport2Json produces
// ~553 cJSON nodes per frame (measured from the 2026-08-10 leak, 48.5M nodes
// over 87.6k frames). Built and freed every frame, exactly as the core does now
// that image_pipe_info_gc deletes it.
static cJSON *build_report(int nodes)
{
  cJSON *root = cJSON_CreateObject();
  cJSON *arr = cJSON_CreateArray();
  cJSON_AddItemToObject(root, "features", arr);
  for (int i = 0; i < nodes / 5; i++)
  {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddItemToArray(arr, o);
    cJSON_AddNumberToObject(o, "x", i * 1.5);
    cJSON_AddNumberToObject(o, "y", i * 2.5);
    cJSON_AddStringToObject(o, "k", "feature");
    cJSON_AddBoolToObject(o, "ok", (i & 1) != 0);
  }
  return root;
}

// A spin that touches memory, so it stands in for real image work rather than
// for an empty loop the compiler could hoist away.
static volatile uint64_t g_sink = 0;
static void spin_ms(double ms, std::vector<uint8_t> &buf)
{
  const uint64_t until = now_us() + (uint64_t)(ms * 1000.0);
  uint64_t acc = 0;
  while (now_us() < until)
    for (size_t i = 0; i < buf.size(); i += 64) acc += buf[i];
  g_sink = acc;
}

struct Item { uint64_t rx_us, enq_us; };

int main(int argc, char **argv)
{
  int stage = 0, seconds = 60, nodes = 553, rate = 37;
  double work_ms = 11.6;
  for (int i = 1; i < argc; i++)
  {
    if (!strcmp(argv[i], "--stage")   && i + 1 < argc) stage   = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--nodes")   && i + 1 < argc) nodes   = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--rate")    && i + 1 < argc) rate    = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--work-ms") && i + 1 < argc) work_ms = atof(argv[++i]);
    else { printf("usage: %s [--stage 0..5] [--seconds N] [--nodes N] "
                  "[--rate N] [--work-ms F]\n", argv[0]); return 2; }
  }
  printf("stage %d  %ds  rate %d/s  nodes %d  work %.1fms\n",
         stage, seconds, rate, nodes, work_ms);

  std::atomic<bool> stop{false};
  Hist h_tick("tick"), h_off("off"), h_wait("wait"), h_e2e("e2e"), h_write("write");

  // Stage 0 is present in every stage: it is the control. If `tick` degrades in
  // a later stage, the whole process is affected and the stage's own numbers
  // cannot be read as that layer's cost.
  std::thread timer([&]{
    raise_qos();
    uint64_t next = now_us() + 2000;
    while (!stop.load(std::memory_order_relaxed))
    {
      const uint64_t t = now_us();
      if (next > t) nap_us(next - t);
      const uint64_t woke = now_us();
      h_tick.add(woke > next ? (woke - next) / 1000.0 : 0.0);
      next += 2000;
      if (next < woke) next = woke + 2000;
    }
  });

  std::mutex qm;
  std::queue<Item> q;
  int wfd = -1;
  if (stage >= 5)
  {
    // A real tty write path without the board: same blocking write(), same
    // driver buffer behaviour, nothing to reset.
    int m = -1, sfd = -1;
    if (openpty(&m, &sfd, NULL, NULL, NULL) == 0)
    {
      wfd = sfd;
      // Something must drain the master, or the buffer fills and every write
      // blocks forever -- which would measure the rig, not the driver.
      std::thread([m]{ char b[4096]; while (read(m, b, sizeof b) > 0) {} }).detach();
    }
    else printf("openpty failed -- stage 5 will not write\n");
  }

  std::thread consumer;
  if (stage >= 1)
  {
    consumer = std::thread([&]{
      raise_qos();
      std::vector<uint8_t> buf(318 * 424);
      for (size_t i = 0; i < buf.size(); i++) buf[i] = (uint8_t)(i * 37);
      while (!stop.load(std::memory_order_relaxed))
      {
        Item it;
        bool got = false;
        { std::lock_guard<std::mutex> g(qm);
          if (!q.empty()) { it = q.front(); q.pop(); got = true; } }
        if (!got) { nap_us(1000); continue; }   // the core's 1ms poll
        const uint64_t t0 = now_us(), c0 = cpu_us();
        h_wait.add((t0 - it.enq_us) / 1000.0);

        if (stage >= 2) { cJSON *r = build_report(nodes); cJSON_Delete(r); }
        if (stage >= 3) spin_ms(work_ms, buf);
        if (stage >= 5 && wfd >= 0)
        {
          const uint64_t w0 = now_us();
          char msg[128];
          int n = snprintf(msg, sizeof msg,
                           "{\"type\":\"report\",\"tid\":%llu,\"cat\":3}\n",
                           (unsigned long long)it.rx_us);
          ssize_t unused = write(wfd, msg, n); (void)unused;
          h_write.add((now_us() - w0) / 1000.0);
        }
        const uint64_t t1 = now_us();
        // The instrument that found the problem in the core: wall minus this
        // thread's OWN cpu over the same span. It cannot separate "descheduled"
        // from "blocked" -- both burn no cpu -- but it does separate either of
        // those from "the work is genuinely expensive", which is the question.
        const double wall = (t1 - t0) / 1000.0, cpu = (cpu_us() - c0) / 1000.0;
        h_off.add(wall > cpu ? wall - cpu : 0.0);
        h_e2e.add((t1 - it.rx_us) / 1000.0);
      }
    });
  }

  // The preview: a second CPU-heavy thread contending for cores and, because it
  // allocates too, for the allocator.
  std::thread preview;
  if (stage >= 4)
  {
    preview = std::thread([&]{
      std::vector<uint8_t> buf(318 * 424);
      while (!stop.load(std::memory_order_relaxed))
      {
        std::vector<uint8_t> tmp(buf);          // allocation, like an encoder
        uint64_t acc = 0;
        for (size_t i = 0; i < tmp.size(); i++) acc += tmp[i];
        g_sink = acc;
        nap_us(125000);                          // 8 fps, as production caps it
      }
    });
  }

  const uint64_t period = 1000000ull / (rate > 0 ? rate : 1);
  const uint64_t t_end = now_us() + (uint64_t)seconds * 1000000ull;
  uint64_t next = now_us();
  while (now_us() < t_end)
  {
    next += period;
    const uint64_t t = now_us();
    if (next > t) nap_us(next - t); else next = now_us();
    if (stage >= 1)
    {
      Item it; it.rx_us = it.enq_us = now_us();
      std::lock_guard<std::mutex> g(qm);
      q.push(it);
    }
  }
  stop.store(true);
  timer.join();
  if (consumer.joinable()) consumer.join();
  if (preview.joinable()) preview.join();

  printf("\n");
  h_tick.print();
  if (stage >= 1) { h_wait.print(); h_off.print(); h_e2e.print(); }
  if (stage >= 5) h_write.print();
  return 0;
}
