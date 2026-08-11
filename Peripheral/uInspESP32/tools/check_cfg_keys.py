#!/usr/bin/env python3
"""Diff the setup-document key tables against the firmware that defines them.

There are four copies of the same knowledge:

    LegacyFirmware.cpp   genMachineSetup / setMachineSetup -- the authority
    tools/uinsp_cfg.py   CFG_GROUP       -- flat<->grouped, for the Python rigs
    UI/WebUI/src/uinspCfg.js  CFG_GROUP  -- the same table, for the browser
    UI/WebUI/src/script.jsx   SETTABLE_KEYS -- which flat names the UI may write

Nothing enforces that they agree, and disagreement is SILENT: `set_setup`
answers `ack: true` whatever it understood, so a command naming a key the
device no longer knows reports success and does nothing. That has already
happened twice -- once when the document was regrouped (eight Python tools
measured whatever the board was carrying and called it the configuration they
had asked for), and once when `auto_rate` / `unanswered_policy` were collapsed
into `skip_policy.mode` (the UI went on writing both dead names until
2026-08-11).

So: run this. It exits non-zero on any disagreement.

    python3 Peripheral/uInspESP32/tools/check_cfg_keys.py

It is deliberately a text scan, not a build step -- it needs no toolchain and
no device, and it answers in well under a second.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))

FW = os.path.join(ROOT, "Peripheral/uInspESP32/src/app/LegacyFirmware.cpp")
PY = os.path.join(HERE, "uinsp_cfg.py")
JS = os.path.join(ROOT, "UI/WebUI/src/uinspCfg.js")
JSX = os.path.join(ROOT, "UI/WebUI/src/script.jsx")

# The C++ locals that hold each group inside genMachineSetup/setMachineSetup.
# Both functions use the same names, which is what makes this scannable.
GROUP_VAR = {"jP": "plate", "jGT": "gate", "jCM": "cam"}

# Emitted-only by construction: derived readouts and advisories, not settings.
# Listing them here is the point -- an unmapped key that is NOT in this list is
# a finding, and adding to this list is a deliberate act with a reason.
READ_ONLY = {
    ("cam", "match_tolerance_mm_eff"),   # derived from the setting + plate speed
    ("skip_policy", "unsafe"),           # advisory flag on the chosen mode
}


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def fw_keys(src):
    """(emitted, settable) as {(group, key)}, scanned from the two functions.

    Sliced by function first: `jP` is reused elsewhere in the file for an
    unrelated pairing report (`jP["slip"]`, `jP["hit"]`...), and scanning the
    whole file would file those as plate settings.
    """
    def body(name):
        """The DEFINITION's body, by brace matching.

        Both functions are forward-declared near the top of the file, and the
        first scan here took the declaration -- whose body is empty, so every
        mapping was reported as naming a key the firmware does not emit. The
        instrument said all 20 long-standing mappings were broken, which is how
        it was caught: a checker that fails everything is failing itself.
        """
        for m in re.finditer(r"void\s+" + re.escape(name) + r"\s*\(", src):
            # Skip the forward declaration: it ends in ';', not '{'.
            p = src.index(")", m.end() - 1)
            rest = src[p + 1:]
            brace = re.match(r"\s*\{", rest)
            if not brace:
                continue
            start = p + 1 + brace.end() - 1
            depth, i = 0, start
            while i < len(src):
                if src[i] == "{":
                    depth += 1
                elif src[i] == "}":
                    depth -= 1
                    if depth == 0:
                        return src[start:i + 1]
                i += 1
            return src[start:]
        raise SystemExit("check_cfg_keys: no definition of %s in LegacyFirmware.cpp" % name)

    gen = body("genMachineSetup")
    setb = body("setMachineSetup")

    emitted = set()
    for var, group in GROUP_VAR.items():
        for k in re.findall(re.escape(var) + r'\["([A-Za-z0-9_]+)"\]\s*=', gen):
            emitted.add((group, k))
    for k in re.findall(r'jSP\["([A-Za-z0-9_]+)"\]\s*=', gen):
        emitted.add(("skip_policy", k))

    settable = set()
    for var, group in GROUP_VAR.items():
        # JSON_SETIF_ABLE(target, jVar, "key")
        for k in re.findall(r'JSON_SETIF_ABLE\([^,]+,\s*' + re.escape(var) +
                            r'\s*,\s*"([A-Za-z0-9_]+)"\)', setb):
            settable.add((group, k))
        # ...and the hand-rolled ones: if(jVar["key"].is<T>())
        for k in re.findall(re.escape(var) + r'\["([A-Za-z0-9_]+)"\]\s*\.\s*is<', setb):
            settable.add((group, k))
    for k in re.findall(r'JSON_SETIF_ABLE\([^,]+,\s*jSP\s*,\s*"([A-Za-z0-9_]+)"\)', setb):
        settable.add(("skip_policy", k))
    for k in re.findall(r'jSP\["([A-Za-z0-9_]+)"\]\s*\.\s*is<', setb):
        settable.add(("skip_policy", k))

    return emitted, settable


def py_table():
    sys.path.insert(0, HERE)
    import uinsp_cfg
    return {flat: tuple(g) for flat, g in uinsp_cfg.CFG_GROUP.items()}


def js_table(src):
    m = re.search(r"export const CFG_GROUP\s*=\s*\{(.*?)\n\};", src, re.S)
    if not m:
        raise SystemExit("check_cfg_keys: could not find CFG_GROUP in uinspCfg.js")
    out = {}
    for flat, g, k in re.findall(
            r'([A-Za-z0-9_]+)\s*:\s*\[\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\]', m.group(1)):
        out[flat] = (g, k)
    return out


def settable_keys(src):
    m = re.search(r"static SETTABLE_KEYS\s*=\s*\[(.*?)\n\s*\];", src, re.S)
    if not m:
        raise SystemExit("check_cfg_keys: could not find SETTABLE_KEYS in script.jsx")
    # Strip // comments before harvesting, so a name mentioned in prose is not
    # mistaken for an entry.
    body = re.sub(r"//[^\n]*", "", m.group(1))
    return set(re.findall(r'"([A-Za-z0-9_]+)"', body))


def main():
    emitted, settable = fw_keys(read(FW))
    py, js = py_table(), js_table(read(JS))
    sk = settable_keys(read(JSX))

    problems = []

    # 1. The two mapping tables must be identical, key for key.
    if py != js:
        for flat in sorted(set(py) | set(js)):
            if py.get(flat) != js.get(flat):
                problems.append("mapping drift  %-24s py=%s js=%s"
                                % (flat, py.get(flat), js.get(flat)))

    # 2. Every mapped name must name something the firmware actually emits.
    #    A mapping to a key that no longer exists regroups a command into a
    #    group the device ignores -- silently.
    for flat, gk in sorted(js.items()):
        if gk[0] in ("plate", "gate", "cam", "skip_policy") and gk not in emitted:
            problems.append("maps to nothing  %-24s -> %s.%s  (firmware emits no such key)"
                            % (flat, gk[0], gk[1]))

    # 3. Every settable firmware key should be reachable from a flat name.
    reachable = set(js.values())
    for gk in sorted(settable - reachable - READ_ONLY):
        problems.append("unreachable      %s.%s is settable on the device but has "
                        "no flat name" % gk)

    # 4. Every SETTABLE_KEYS entry must be a flat name the firmware knows --
    #    either mapped into a group, or a genuine top-level key.
    flat_top = set(re.findall(r'JSON_SETIF_ABLE\([^,]+,\s*jdoc\s*,\s*"([A-Za-z0-9_]+)"\)',
                              read(FW)))
    flat_top |= {"machine_id", "CAM1_Tags", "CAM2_Tags", "persist",
                 "stage_pulse_offset", "stage_pulse_width_us",
                 "stage_pulse_center", "io_on_level"}
    for k in sorted(sk):
        if k not in js and k not in flat_top:
            problems.append("dead key         %-24s in SETTABLE_KEYS but the firmware "
                            "has no such setting" % k)

    # 5. A name that is mapped but never listed can be displayed and never
    #    written -- the failure that hid min_detect_dist_um.
    for flat in sorted(js):
        if flat not in sk:
            problems.append("display-only     %-24s is mapped but missing from "
                            "SETTABLE_KEYS" % flat)

    if problems:
        print("check_cfg_keys: %d problem(s)\n" % len(problems))
        for p in problems:
            print("  " + p)
        return 1

    print("check_cfg_keys: OK -- %d mapped names, %d settable firmware keys, "
          "%d UI-writable names agree" % (len(js), len(settable), len(sk)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
