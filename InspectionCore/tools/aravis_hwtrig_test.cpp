// Does the SHIPPED CameraLayer_Aravis catch every hardware trigger?
//
// A python Aravis probe only answers "can this camera do it"; this answers
// "does our layer do it", which is the only version that matters. The layer
// has no state readback of its own worth trusting, so the check is empirical:
// fire N pulses, count EV_IMG, and read frame_id continuity -- contiguous ids
// with missing frames means the camera REFUSED triggers it could not service
// (it does so silently), gaps mean they were exposed and lost in transit.
//
// Needs the uinsp panel on 127.0.0.1:8765 to source the pulses (ESP32-timed;
// the panel's own thread jitters 60-90 ms, fine for counting, useless for
// pairing). Build against a FRESH libCameraLayer.a -- a stale archive silently
// tests two-month-old code and reads exactly like a hardware fault:
//
//   B=InspectionCore; cmake --build $B/build/mac-arm64 --target CameraLayer
//   clang++ -std=c++17 -O1 tools/aravis_hwtrig_test.cpp -o /tmp/hwtrig \
//     -I$B/CameraLayer/include -I$B/common_lib/include -I$B/logctrl/include \
//     -I$B/acvImage/include -I$B/smem_channel/include \
//     $(pkg-config --cflags --libs aravis-0.8) \
//     $B/build/mac-arm64/lib{CameraLayer,logctrl,acvImage,common_lib,smem_channel}.a
//   /tmp/hwtrig [pulses] [hz] [reps]      # SKIP_TM=1 to skip TriggerMode(2)
//
// Baseline on MV-CA050-12UC over USB3 (ResultingFrameRate 24.9 at full frame
// BGR8): 11 consecutive reps of 40/40 at 5 Hz, and 40/40 at 15 Hz.

#include <CameraLayer_Aravis.hpp>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <thread>
#include <chrono>

static int g_img = 0, g_err = 0, g_lost = 0;
static std::vector<uint32_t> g_ids;

static CameraLayer::status cb(CameraLayer &cl, int type, void *ctx)
{
  if (type == CameraLayer::EV_IMG)
  {
    CameraLayer::frameInfo fi = cl.GetFrameInfo();
    if (fi.frameNumValid) g_ids.push_back(fi.frameNum);
    g_img++;
  }
  else if (type == CameraLayer::EV_ERROR)   g_err++;
  else if (type == CameraLayer::EV_CTRL_LOST) g_lost++;
  return CameraLayer::ACK;
}

static void fire_burst(int count, double hz, int width_us)
{
  char cmd[512];
  snprintf(cmd, sizeof(cmd),
           "curl -s -m 30 -X POST http://127.0.0.1:8765/camtest "
           "-H 'Content-Type: application/json' "
           "-d '{\"mode\":\"burst\",\"count\":%d,\"hz\":%.2f,\"cpin\":17,"
           "\"lpin\":18,\"light_delay\":0,\"light_duration\":%d}' >/dev/null",
           count, hz, width_us);
  int rc = system(cmd);
  printf("  [trig] %d pulses @ %.1f Hz (rc=%d)\n", count, hz, rc);
}

int main(int argc, char **argv)
{
  const int    pulses = (argc > 1) ? atoi(argv[1]) : 40;
  const double hz     = (argc > 2) ? atof(argv[2]) : 5.0;
  const int    reps   = (argc > 3) ? atoi(argv[3]) : 3;

  std::vector<CameraLayer::BasicCameraInfo> devs;
  CameraLayer_Aravis::listAddDevices(devs);
  if (devs.empty()) { printf("no camera attached\n"); return 2; }

  int worst = pulses;
  for (int r = 0; r < reps; r++)
  {
    g_img = g_err = g_lost = 0;
    g_ids.clear();

    CameraLayer_Aravis cam(devs[0], "", cb, NULL);
    // The ctor already configures triggering inline. Calling TriggerMode(2) on
    // top of it is what CameraSetup does -- test both, they are not the same
    // path: TriggerMode() stops and restarts acquisition mid-flight.
    const bool call_tm = getenv("SKIP_TM") == NULL;
    if (call_tm) cam.TriggerMode(2);
    printf("  [cfg] TriggerMode(2) %s\n", call_tm ? "called" : "SKIPPED");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    g_img = 0; g_ids.clear();                 // drop anything from settling

    fire_burst(pulses, hz, 600);
    const auto until = std::chrono::steady_clock::now() +
                       std::chrono::milliseconds((int)(pulses / hz * 1000) + 3000);
    while (std::chrono::steady_clock::now() < until)
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

    int span = g_ids.empty() ? 0 : (int)(g_ids.back() - g_ids.front() + 1);
    printf("  >>> rep %d: %d/%d frames  frame_id span=%d gaps=%d  err=%d lost=%d\n",
           r, g_img, pulses, span, span - (int)g_ids.size(), g_err, g_lost);
    if (g_img < worst) worst = g_img;
  }

  printf("== worst rep: %d/%d\n", worst, pulses);
  return worst == pulses ? 0 : 1;
}
