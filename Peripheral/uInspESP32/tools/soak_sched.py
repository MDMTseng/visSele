#!/usr/bin/env python3
"""Run the soak queue: one theme per run, each with a prediction that can fail.

A soak that just "runs the machine for N hours and sees nothing" proves very
little -- it is one sample of one operating point, and it cannot say which of
the many things that could have failed were even exercised. The queue here is
the opposite shape: each run states ONE claim, drives the machine to the regime
where that claim would break if it were false, and returns pass/fail on a
number rather than on an absence of noise.

Every run gets a FRESH CORE. That is not tidiness -- the fault-injection env
vars (INSP_PERIF_VERDICT_PATTERN and friends) are read at static init, so a run
that needs them and a run that must not have them cannot share a process. A
fresh core per run also means no cross-run contamination of the pairing state,
which is exactly the contamination dryrun_pairing.py hard-resets the board to
avoid.

Exclusivity is the other reason this is a sequential runner and not a cron job.
The core owns the serial port; two overlapping runs do not degrade gracefully,
they fight over the board. Cron cannot express "not while the previous one is
still going" -- a queue can.

Between runs the board is checked for standstill and a clear error list. A
queue that keeps running after the machine has gone bad produces hours of
measurements of a broken machine, so a failed health check STOPS the queue.

  python3 soak_sched.py --list
  python3 soak_sched.py --block A            # unattended: no parts needed
  python3 soak_sched.py --only jitter,burst
"""
import argparse, json, os, signal, socket, subprocess, sys, time, datetime

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
CORE_DIR = os.path.join(REPO, "InspectionCore", "Core0_1")
CORE_BIN = os.path.join(REPO, "InspectionCore", "build", "mac-arm64", "visSele")
WS_PORT = 4090        # BPG WebSocket -- binds early, so "listening" != "ready"
PORT = 4099           # INSP_PERIF_CONSOLE: verbatim device bytes, no framing.
                      # Only exists if the core was launched with the env var
                      # set, which is why core_start sets it rather than
                      # inheriting it.
CONN = {"type": "CONNECT", "uart_name": "/dev/cu.usbserial-0001",
        "baudrate": 230400, "machine_type": "uInspESP32",
        # cat IS the severity class and SMALLER IS WORSE.
        "cat_ok": 3, "cat_ng": 1, "cam_idx": 1, "pairing": "timestamp"}

VERDICT_SEED = 20260806


# --------------------------------------------------------------------------
# The queue. `argv` is relative to this directory; `needs_parts` decides the
# block; `env` is applied on top of a cleaned environment for that run's core.
#
# `why` is the claim. If a run cannot fail, it does not belong here.
# --------------------------------------------------------------------------
RUNS = [
    dict(
        name="jitter", block="A", minutes=12,
        why="配對的斷點在視窗,不在負載。抖動掃到超過視窗之前 disagree 必須是 0;"
            "超過之後必須開始失配 —— 兩邊都要成立,只有前半段成立代表視窗根本沒被逼到。",
        argv=["jitter_sweep.py", "--seconds", "60",
              "--jitters", "0", "1000", "2500", "4000", "6000", "10000"],
        # Without the pattern the core answers NA to everything, so every
        # object reads as BAD and "zero misplaced verdicts" -- the whole pass
        # condition -- cannot fail. A vacuous pass is worse than no run.
        env={"INSP_PERIF_VERDICT_PATTERN": str(VERDICT_SEED)},
    ),
    dict(
        name="jitter_edge", block="A", minutes=8,
        why="上面那個掃描通過了,但沒有逼到視窗,所以只證明了判準的前半段。"
            "算術很清楚:標稱 40000us 加減 10000 之後最小間距還有 24736us,離 5000us 的視窗差五倍。"
            "要讓鄰居真的進到視窗內,抖動必須逼近 40000-5000=35000us。"
            "這一列掃的就是那個區間 —— 斷點若不落在視窗附近,那視窗就不是限制因素,"
            "而『2xTOL <= min_detect_sep』這條護欄擋的是別的東西。",
        argv=["jitter_sweep.py", "--seconds", "60",
              "--jitters", "20000", "30000", "34000", "37000"],
        env={"INSP_PERIF_VERDICT_PATTERN": str(VERDICT_SEED)},
    ),
    # Report-timestamp noise. One run per half-width, because the env var is
    # read at static init and the scheduler gives each run its own core -- which
    # is the whole reason a sweep of this shape is cheap here.
    #
    # 5000us is the match window. Below it nothing may be mis-sorted and nothing
    # should even halt; above it halts are the DESIGNED behaviour (refusing to
    # answer is correct) but a mis-sort never is. The pass condition is the same
    # at every width: zero misplaced verdicts.
] + [
    # 主體:高斯。維護迴路積分的就是這個 —— 每份被接受的報告推一下 offset。
    # 判準比「沒有錯置」更嚴:sigma 遠小於視窗時連停機都不該有,
    # 若停機了,代表推擠沒有互相抵銷,時鐘被自己的維護走掉了。
    dict(
        name="tsn_g%d" % s, block="A", minutes=5,
        why=("報告時間戳加 sigma=%dus 的高斯噪音(視窗 5000us)。"
             "固定偏移測不到這個 —— gate() 從每份被接受的報告重新量測 offset_us,"
             "固定量正是它設計來追掉的。零均值噪音學不走,"
             "所以這是循環式維護穩不穩的直接測試:推擠互相抵銷,還是隨機遊走。" % s),
        argv=["jitter_sweep.py", "--seconds", "60", "--jitters", "0"],
        env={"INSP_PERIF_VERDICT_PATTERN": str(VERDICT_SEED),
             "INSP_PERIF_FAULT_TS_NOISE_US": str(s),
             "INSP_PERIF_FAULT_TS_NOISE_SEED": str(VERDICT_SEED)},
    )
    for s in (200, 1000, 2500)
] + [
    # 椒鹽:偶發的大偏離。高斯的尾巴在 60 秒裡出現得太少,取樣不到;
    # 而真正會造成錯置的正是這種一次性的大偏離,不是主體分佈。
    # 把發生率變成可設定的,買到的是等自然事件買不到的統計量。
    dict(
        name="tsn_s%d" % k, block="A", minutes=5,
        why=("每 20 份報告有一份被推 +-%dus(視窗 5000us),符號隨機 —— "
             "來得太晚和太早失敗的方式不同,能被混淆的鄰居在兩側。"
             "視窗以內不該有任何影響;視窗以外必須是拒絕回答(停機),"
             "而**絕不能**是把判定發給鄰居。" % k),
        argv=["jitter_sweep.py", "--seconds", "60", "--jitters", "0"],
        env={"INSP_PERIF_VERDICT_PATTERN": str(VERDICT_SEED),
             "INSP_PERIF_FAULT_TS_SPIKE_US": str(k),
             "INSP_PERIF_FAULT_TS_SPIKE_EVERY": "20",
             "INSP_PERIF_FAULT_TS_NOISE_SEED": str(VERDICT_SEED)},
    )
    for k in (3000, 6000, 20000)
] + [
    dict(
        name="tsn_mix", block="A", minutes=5,
        why="兩者一起:sigma=1000us 的主體加上每 20 份一次的 +-6000us 大偏離。"
            "分開跑各自乾淨不代表合起來乾淨 —— 主體會讓 offset 持續微動,"
            "大偏離則要在那個已經在動的時鐘上被判斷,這才是產線的形狀。",
        argv=["jitter_sweep.py", "--seconds", "60", "--jitters", "0"],
        env={"INSP_PERIF_VERDICT_PATTERN": str(VERDICT_SEED),
             "INSP_PERIF_FAULT_TS_NOISE_US": "1000",
             "INSP_PERIF_FAULT_TS_SPIKE_US": "6000",
             "INSP_PERIF_FAULT_TS_SPIKE_EVERY": "20",
             "INSP_PERIF_FAULT_TS_NOISE_SEED": str(VERDICT_SEED)},
    ),
    dict(
        name="burst", block="A", minutes=16,
        why="時間戳配對撐得過『沖垮相機 -> 排空 -> 再沖』的過渡。穩定速率是簡單形狀,"
            "轉態才是序數配對會偷偷錯一格的地方。判準:disagree 0 且無 CAM_CLOCK_LOST。",
        argv=["burst_pairing.py", "--seconds", "900", "--min-sep-us", "28571"],
        env={}, pass_if_stdout="clean", fail_if_stdout="FAIL",
    ),
    dict(
        name="recal", block="A", minutes=32,
        why="每次 RECAL 掉 96 bytes 是暫態還是洩漏。四分鐘看到的四步分不出來;"
            "半小時的 min_heap 若單調下降就是洩漏,若回彈就是配置器的暫態。",
        argv=["recal_leak.py", "--minutes", "30"],
        env={},
    ),
    dict(
        name="phantom", block="A", minutes=20,
        why="T-7 之後 newPulseEvent 只剩一個生產者。在相機天花板附近連續打幻影,"
            "ph_drop 與 tqovf 必須維持 0 —— 不是『沒看到問題』,是這兩個計數器就是為此存在的。",
        argv=["burst_pairing.py", "--seconds", "1080", "--min-sep-us", "28571",
              "--hz-lo", "1.2", "--hz-hi", "3.0"],
        env={}, pass_if_stdout="clean", fail_if_stdout="FAIL",
        watch=["ph_drop", "tqovf", "tqburst"],
    ),
    # ---- Block B: 需要盤上有料 -------------------------------------------
    dict(
        name="slip", block="B", minutes=10,
        why="判定有沒有落到錯的物件上。agree/disagree 只是兩套配對互比,"
            "總數則完全看不到滑移 —— 錯一格的總數和正確的一模一樣。"
            "這一項要真料件,而且要噪音判定,規則圖樣會把等於其週期的滑移藏起來。",
        argv=["slip_probe.py", "--real", "--seconds", "480",
              "--verdict-seed", str(VERDICT_SEED)],
        env={"INSP_PERIF_VERDICT_PATTERN": str(VERDICT_SEED)},
        needs_parts=True,
    ),
    dict(
        name="real", block="B", minutes=125,
        why="殘差的分佈,不只是最大值。視窗必須高過合法發生的最壞殘差,"
            "四分鐘的 delta_max 說不出一整天會遇到的尾巴,也分不出"
            "『一顆 240us 其餘都在 60 以下』和『經常接近 240』—— 這兩者要的視窗完全不同。",
        argv=["soak_real.py", "--hours", "2", "--report-every", "300"],
        env={}, needs_parts=True,
    ),
    dict(
        name="regress", block="A", minutes=185,
        why="2026-08-06 修掉的每一項都只是『改完沒再看到』,而它取代的故障要 10-30 分鐘才復發。"
            "唯一能證明的是時間。三小時的看守排最後,因為它最長,前面每一項都比它先給出答案。",
        argv=["regress_watch.py", "--hours", "3", "--report-every", "600",
              "--verdict-seed", str(VERDICT_SEED)],
        # Same requirement as jitter: the slip check needs the seeded verdicts,
        # and regress_watch refuses to call itself a pass without them.
        env={"INSP_PERIF_VERDICT_PATTERN": str(VERDICT_SEED)},
    ),
]


# The core owned by the run in flight. An interrupted queue used to leave it
# running: the exception propagates out of run_one, past the core_stop calls,
# and a core nobody is watching keeps the serial port and can keep the plate
# turning. Tracked here so the handler can shut it down.
CUR_CORE = (None, None)


def log(fh, msg):
    line = "%s %s" % (datetime.datetime.now().strftime("%H:%M:%S"), msg)
    print(line, flush=True)
    fh.write(line + "\n")
    fh.flush()


# --------------------------------------------------------------------------
# core lifecycle. Never SIGKILL: the camera is left wedged by a hard kill and
# the next run opens a camera that will not stream.
# --------------------------------------------------------------------------
def core_start(env_extra, logpath):
    env = dict(os.environ)
    # Fault injection is read at static init, so a stale export from an earlier
    # session silently changes what every run below measures. Clear the lot,
    # then put back the one this harness cannot work without.
    for k in list(env):
        if k.startswith("INSP_PERIF_"):
            del env[k]
    env["INSP_PERIF_CONSOLE"] = str(PORT)
    env.update(env_extra or {})
    fh = open(logpath, "w")
    p = subprocess.Popen([CORE_BIN], cwd=CORE_DIR, env=env,
                         stdout=fh, stderr=subprocess.STDOUT)
    # 4090 binds before the camera is up, so reaching it is not proof of ready.
    # 4099 is what the runs actually speak, so wait for that one too.
    deadline = time.time() + 90
    got_ws = False
    while time.time() < deadline:
        time.sleep(0.5)
        if p.poll() is not None:
            return None, fh, "core exited early rc=%s" % p.returncode
        try:
            socket.create_connection(("127.0.0.1", WS_PORT if not got_ws else PORT),
                                     timeout=1).close()
            if not got_ws:
                got_ws = True
                continue
            time.sleep(6.0)          # camera open + first frames
            return p, fh, None
        except OSError:
            pass
    return None, fh, "core never listened on %d" % (PORT if got_ws else WS_PORT)


def core_stop(p, fh):
    if p is None:
        return
    try:
        p.send_signal(signal.SIGINT)
        for _ in range(60):
            if p.poll() is not None:
                break
            time.sleep(0.5)
        if p.poll() is None:
            p.terminate()
            for _ in range(40):
                if p.poll() is not None:
                    break
                time.sleep(0.5)
    except Exception:
        pass
    try:
        fh.close()
    except Exception:
        pass


# --------------------------------------------------------------------------
# board health, over the core's peripheral channel
# --------------------------------------------------------------------------
def board_rpc(cmds, wait=2.0):
    s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
    s.settimeout(0.4)
    # `!pd` addresses the CORE; everything else on this socket goes to the
    # DEVICE. The peripheral channel does not exist until a client CONNECTs it,
    # and a headless core opens nothing -- so a bare CONNECT here is answered
    # with "no perif channel" and every later question reads as a dead board.
    s.sendall(("!pd " + json.dumps(CONN) + "\n").encode())
    # CONNECT reopens the UART, which toggles DTR and resets the ESP32. Asking
    # it anything before it has finished booting also reads as a dead board.
    time.sleep(3.5)
    for c in cmds:
        s.sendall((json.dumps(c) + "\n").encode())
        time.sleep(0.4)   # back-to-back commands: the second one gets dropped
    out, t0 = [], time.time()
    buf = b""
    while time.time() - t0 < wait:
        try:
            buf += s.recv(65536)
        except socket.timeout:
            continue
        except OSError:
            break
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            line = line.strip()
            if line.startswith(b"{"):
                try:
                    out.append(json.loads(line.split(b"*")[0].decode()))
                except Exception:
                    pass
    s.close()
    return out


def health(fh):
    """Standstill + clear error list. Returns (ok, description)."""
    poll = stat = None
    for attempt in range(3):
        try:
            reps = board_rpc([{"type": "poll"}, {"type": "get_running_stat"}], 5.0)
        except OSError as e:
            return False, "cannot reach core: %s" % e
        poll = next((r for r in reps if r.get("type") == "poll"), None)
        # `error_hist` alone is NOT enough to identify the get_running_stat
        # reply: the periodic status line carries that key too, in a ~100-byte
        # object with no health block. Matching it read min_heap as absent and
        # would have read the error list off the wrong message. Require the
        # blocks that only the real reply has, and take the last one.
        stat = next((r for r in reversed(reps)
                     if "health" in r and "cam_sync" in r), None)
        if poll is not None:
            break
        time.sleep(2.0)
    if poll is None:
        return False, "no poll reply after 3 tries"
    bad = []
    # plate_freq is the SETPOINT; plate_freq_meas is what the plate is doing.
    # Every earlier "stopped" confirmation in this project read the setpoint.
    if poll.get("plate_freq_meas", 0) != 0:
        bad.append("plate still turning (meas=%s)" % poll["plate_freq_meas"])
    if poll.get("nerr", 0):
        bad.append("nerr=%s" % poll["nerr"])
    if stat and stat.get("error_hist"):
        bad.append("error_hist=%s" % stat["error_hist"][:3])
    desc = "state=%s meas=%s q=%s ph_drop=%s tqovf=%s heap_min=%s" % (
        poll.get("state"), poll.get("plate_freq_meas"), poll.get("q"),
        poll.get("ph_drop"), poll.get("tqovf"),
        (stat or {}).get("health", {}).get("min_heap"))
    return (not bad), (desc if not bad else desc + "  BAD: " + "; ".join(bad))


def counters(keys):
    try:
        reps = board_rpc([{"type": "poll"}], 2.0)
    except OSError:
        return {}
    poll = next((r for r in reps if r.get("type") == "poll"), {})
    return {k: poll.get(k) for k in keys}


# --------------------------------------------------------------------------
def run_one(r, outdir, fh):
    logpath = os.path.join(outdir, "%s.core.log" % r["name"])
    runlog = os.path.join(outdir, "%s.log" % r["name"])
    log(fh, "")
    log(fh, "=== %s  (~%d min, block %s)" % (r["name"], r["minutes"], r["block"]))
    for line in r["why"].split("。"):
        if line.strip():
            log(fh, "    %s。" % line.strip())

    global CUR_CORE
    p, cfh, err = core_start(r.get("env"), logpath)
    CUR_CORE = (p, cfh)
    if err:
        core_stop(p, cfh)
        log(fh, "  core: %s -> SKIP" % err)
        return "CORE_FAIL", {}

    ok, desc = health(fh)
    log(fh, "  before: %s" % desc)
    if not ok:
        core_stop(p, cfh)
        return "UNHEALTHY_BEFORE", {}

    before = counters(r["watch"]) if r.get("watch") else {}

    t0 = time.time()
    with open(runlog, "w") as rf:
        # -u: child stdout is block-buffered when redirected to a file, so a
        # three-hour run shows nothing at all until it exits. That is fine for
        # the verdict and useless for watching -- if a long run wedges at hour
        # two, a buffered log cannot say where.
        proc = subprocess.run([sys.executable, "-u"] + r["argv"], cwd=HERE,
                              stdout=rf, stderr=subprocess.STDOUT)
    dt = time.time() - t0
    out = open(runlog, encoding="utf8", errors="replace").read()

    verdict = "PASS" if proc.returncode == 0 else "FAIL(rc=%d)" % proc.returncode
    # Some tools print their verdict and still exit 0.
    if r.get("fail_if_stdout") and r["fail_if_stdout"] in out:
        verdict = "FAIL(stdout)"
    elif r.get("pass_if_stdout") and r["pass_if_stdout"] not in out:
        verdict = "FAIL(no '%s' in output)" % r["pass_if_stdout"]

    delta = {}
    if r.get("watch"):
        after = counters(r["watch"])
        for k in r["watch"]:
            a, b = after.get(k), before.get(k)
            if isinstance(a, int) and isinstance(b, int):
                delta[k] = a - b
                if a - b:
                    verdict = "FAIL(%s +%d)" % (k, a - b)
        log(fh, "  counters: %s" % delta)

    ok2, desc2 = health(fh)
    log(fh, "  after:  %s" % desc2)
    core_stop(p, cfh)

    tail = [l for l in out.splitlines() if l.strip()][-6:]
    for l in tail:
        log(fh, "  | %s" % l[:150])
    log(fh, "  %s  (%.1f min)  -> %s" % (r["name"], dt / 60.0, verdict))
    if not ok2 and verdict.startswith("PASS"):
        verdict = "PASS_BUT_UNHEALTHY_AFTER"
    return verdict, delta


def main(a):
    sel = RUNS
    if a.block:
        sel = [r for r in sel if r["block"] == a.block.upper()]
    if a.only:
        want = [x.strip() for x in a.only.split(",")]
        sel = [r for r in sel if r["name"] in want]
    if a.list or not sel:
        total = 0
        for r in RUNS:
            total += r["minutes"]
            print("  %-8s block %s  ~%3d min  %s" % (
                r["name"], r["block"], r["minutes"],
                "需要料件" if r.get("needs_parts") else "免料件"))
            print("           %s" % r["why"].replace("\n", ""))
        print("  total ~%.1f h" % (total / 60.0))
        return 0

    stamp = datetime.datetime.now().strftime("%Y-%m-%d_%H%M")
    outdir = os.path.join(HERE, "soak_runs", stamp)
    os.makedirs(outdir, exist_ok=True)
    fh = open(os.path.join(outdir, "queue.log"), "w")

    log(fh, "queue: %s" % ", ".join(r["name"] for r in sel))
    log(fh, "est %.1f h   out %s" % (sum(r["minutes"] for r in sel) / 60.0, outdir))
    results = []
    stopped = None
    for r in sel:
        try:
            v, d = run_one(r, outdir, fh)
        except KeyboardInterrupt:
            log(fh, "interrupted during %s -- stopping its core" % r["name"])
            core_stop(*CUR_CORE)
            stopped = "interrupted"
            break
        results.append((r["name"], v))
        if v.startswith("UNHEALTHY") or v == "CORE_FAIL":
            # Measuring a broken machine for another four hours teaches nothing.
            log(fh, "STOPPING QUEUE: %s left the machine in %s" % (r["name"], v))
            stopped = v
            break

    log(fh, "")
    log(fh, "===== summary =====")
    for n, v in results:
        log(fh, "  %-8s %s" % (n, v))
    if stopped:
        log(fh, "  queue stopped early: %s" % stopped)
    remaining = [r["name"] for r in sel if r["name"] not in dict(results)]
    if remaining:
        log(fh, "  not run: %s" % ", ".join(remaining))
    fh.close()
    return 0 if all(v.startswith("PASS") for _, v in results) and not stopped else 1


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt))
    ap = argparse.ArgumentParser()
    ap.add_argument("--block", help="A = 免料件(可無人值守), B = 需要料件")
    ap.add_argument("--only", help="comma-separated run names")
    ap.add_argument("--list", action="store_true")
    sys.exit(main(ap.parse_args()))
