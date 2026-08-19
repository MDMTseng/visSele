// AUDIT_BACKLOG_2026-08-18 P1: "unbounded write past dataBuff" -- verification.
//
// The report says tryRecoverResetFromErrorBuffer returns without compacting
// when firstBrace <= 0, leaving buffIdx pinned at 2048 while recv_data keeps
// doing `dataBuff[buffIdx++]=c` with no pre-write check. It cites lines
// 104-121. That range is real and the missing compaction inside it is real.
// What the cited range does not include is the tail of the same function:
//
//     if(firstBrace>0)                     { ...compact... }
//     else if(buffIdx>=sizeof(dataBuff))   { buffIdx=0; }     // <-- line 136
//
// which is exactly the case the report describes. So the question is not
// whether that one branch compacts, but whether ANY path gets a byte written at
// index >= 2048. This looks for one.
//
// Detection is belt and braces: a canary block placed immediately after the
// layer object and checked after EVERY byte, plus a per-byte assertion that
// buffIdx never leaves [0, 2048]. Checking per byte matters -- an overrun that
// a later compaction tidies up would be invisible to an end-of-run check.
//
// Host build, NO BOARD and no core required -- unlike everything else in this
// directory. The only Arduino symbol the protocol layer uses is millis(), which
// tools/host_stub/Arduino.h supplies.
//
// BUILD + RUN (MSYS2 MinGW64; any host g++ works):
//   cd Peripheral/uInspESP32
//   g++ -std=c++17 -O0 -g -Itools/host_stub -Iinclude -Iinclude/comm //     -o /tmp/t.exe tools/test_data_layer_overflow.cpp //     src/comm/Data_Layer_Protocol.cpp src/comm/json_seg_parser.cpp
//   /tmp/t.exe     # exit 0 = pass
#include "comm/Data_Layer_Protocol.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdint>

class Probe : public Data_JsonRaw_Layer {
public:
  void connected(Data_Layer_IF *) override {}
  int  recv_RESET() override { nReset++; return 0; }
  int  recv_ERROR(ERROR_TYPE, uint8_t * = NULL, size_t = 0) override { nErr++; return 0; }
  int  cap() const { return (int)sizeof(dataBuff); }
  int  idx() const { return buffIdx; }
  int  nErr = 0, nReset = 0;
};

struct Harness {
  Probe   p;
  uint8_t canary[512];
  Harness() { memset(canary, 0xA5, sizeof canary); }
  bool canaryIntact() const {
    for (size_t i = 0; i < sizeof canary; i++) if (canary[i] != 0xA5) return false;
    return true;
  }
};

static int failures = 0;
static int maxIdxSeen = 0;

static bool feed(Harness &h, const std::string &s, const char *what)
{
  for (size_t k = 0; k < s.size(); k++) {
    uint8_t b = (uint8_t)s[k];
    h.p.recv_data(&b, 1, false);
    if (h.p.idx() > maxIdxSeen) maxIdxSeen = h.p.idx();
    if (h.p.idx() > h.p.cap() || h.p.idx() < 0) {
      printf("  [FAIL] %-40s buffIdx=%d after byte %zu/%zu\n",
             what, h.p.idx(), k + 1, s.size());
      failures++; return false;
    }
    if (!h.canaryIntact()) {
      printf("  [FAIL] %-40s canary clobbered at byte %zu\n", what, k + 1);
      failures++; return false;
    }
  }
  return true;
}

static void run(const char *what, const std::string &s)
{
  Harness h;
  if (feed(h, s, what))
    printf("  [ ok ] %-40s buffIdx=%-5d err=%d rst=%d\n",
           what, h.p.idx(), h.p.nErr, h.p.nReset);
}

int main()
{
  const int CAP = 2048;
  const int OVER = CAP * 3;

  printf("targeted -- the shapes the report names:\n");
  run("'{' at index 0, no other brace",   "{" + std::string(OVER, 'A'));
  run("'[' open, no '{' at all",          "[" + std::string(OVER, 'B'));
  run("0x1 JSONRAW open, junk body",      std::string(1, '\x01') + std::string(OVER, 'C'));

  printf("\nadjacent shapes -- same branch, different way in:\n");
  run("all '{'",                          std::string(OVER, '{'));
  run("'{' then partial RESET, no close", "{\"type\":\"RESET\"" + std::string(OVER, 'E'));
  run("'{' then partial clear_error",     "{\"type\":\"clear_error\"" + std::string(OVER, 'F'));
  run("nested braces, never closed",      std::string(OVER / 2, '{') + std::string(OVER / 2, 'G'));
  run("brace only at the very end",       std::string(OVER, 'H') + "{");
  run("NULs (buffer is not a C string)",  std::string(1, '{') + std::string(OVER, '\0'));
  run("0x1 then a brace far in",          std::string(1, '\x01') + std::string(1500, 'J')
                                          + "{" + std::string(OVER, 'K'));
  {
    std::string s;
    for (int i = 0; i < OVER; i++) s += (i % 977 == 0) ? '{' : 'I';
    run("sparse braces every 977 bytes", s);
  }

  printf("\nrecovery interleaved -- RESET mid-overflow:\n");
  run("fill, then a complete RESET",      "{" + std::string(CAP + 100, 'L') + "{\"type\":\"RESET\"}");
  run("fill, then complete clear_error",  "{" + std::string(CAP + 100, 'M') + "{\"type\":\"clear_error\"}");
  run("RESET straddling the cap",         "{" + std::string(CAP - 8, 'N')
                                          + "{\"type\":\"RESET\"}" + std::string(500, 'O'));

  printf("\nrandom fuzz, biased to the interesting bytes:\n");
  const char alphabet[] = "{}[]\"\\:,\x01\n\r RESETclear_or0123";
  uint32_t st = 0x13572468;
  int trials = 0;
  for (; trials < 400 && !failures; trials++) {
    std::string s; s.reserve(6000);
    for (int i = 0; i < 6000; i++) {
      st = st * 1664525u + 1013904223u;
      s += alphabet[(st >> 16) % (sizeof(alphabet) - 1)];
    }
    Harness h;
    char nm[48]; snprintf(nm, sizeof nm, "fuzz trial %d", trials);
    if (!feed(h, s, nm)) break;
  }
  if (!failures) printf("  [ ok ] %d trials x 6000 bytes, no overrun\n", trials);

  printf("\nhighest buffIdx observed: %d (array is %d)\n", maxIdxSeen, CAP);
  printf("%s\n", failures
    ? "P1 CONFIRMED -- a write past dataBuff is reachable"
    : "P1 NOT REPRODUCED -- see the else-if at Data_Layer_Protocol.cpp:136");
  return failures ? 1 : 0;
}
