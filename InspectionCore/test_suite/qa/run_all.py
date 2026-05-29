#!/usr/bin/env python3
"""Discover and run all qa_*.py modules in this dir. Exit = total failures."""
import glob, importlib.util, os, sys

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
total = 0
mods = sorted(glob.glob(os.path.join(here, "qa_*.py")))
if not mods:
    print("no qa_*.py modules yet"); sys.exit(0)
for path in mods:
    name = os.path.splitext(os.path.basename(path))[0]
    spec = importlib.util.spec_from_file_location(name, path)
    m = spec.loader.load_module() if hasattr(spec.loader, "load_module") else None
    if m is None:
        mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod); m = mod
    cases = getattr(m, "CASES", None)
    if cases is None:
        print(f"{name}: no CASES"); continue
    from qalib import run_module
    total += run_module(name, cases)
print(f"\n==== TOTAL FAILURES: {total} ====")
sys.exit(total)
