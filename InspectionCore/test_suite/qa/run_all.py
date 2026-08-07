#!/usr/bin/env python3
"""Run every qa_*.py in this dir. Exit code = total failures.

Each module runs as its OWN PROCESS, and the contract is one line:

    exit code = number of failing cases (0 = all pass)

which every module already honours via `sys.exit(run_module(...))` or its own
`sys.exit(fails)`.

Subprocesses, not imports, because importing them shared one interpreter with
the runner and two things followed from that:

  1. A module that runs at import time and calls sys.exit() -- qa_insp_region
     and qa_objdetect_dark are written that way, deliberately, so they can also
     be run by hand -- raised SystemExit straight through the runner and ENDED
     THE WHOLE SUITE. Alphabetically that put qa_measure, qa_parse and
     qa_system after a wall: they were never executed here, and nothing said
     so. The suite printed a couple of green modules and stopped.

  2. An import-time assertion (qa_imgstress builds its golden baseline at
     import) killed the run for the same reason.

A process boundary also means a segfault in one module costs that module, not
the report for everything after it.
"""
import glob, os, subprocess, sys

here = os.path.dirname(os.path.abspath(__file__))
mods = sorted(glob.glob(os.path.join(here, "qa_*.py")))
if not mods:
    print("no qa_*.py modules yet")
    sys.exit(0)

total = 0
summary = []
for path in mods:
    name = os.path.splitext(os.path.basename(path))[0]
    r = subprocess.run([sys.executable, path], cwd=here)
    rc = r.returncode
    # A crash (negative rc = killed by signal) is not a count of anything --
    # report it as one failure and say what it was, rather than adding a
    # nonsense number to the total.
    if rc < 0:
        summary.append(f"{name}: KILLED by signal {-rc}")
        total += 1
    else:
        total += rc
        summary.append(f"{name}: {rc} failure(s)" if rc else f"{name}: pass")

print("\n==== SUMMARY ====")
for s in summary:
    print("  " + s)
print(f"==== TOTAL FAILURES: {total} ====")
sys.exit(total)
