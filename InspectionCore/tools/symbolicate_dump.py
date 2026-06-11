#!/usr/bin/env python
"""Symbolicate an InspectionCore crash_<utc>.dump stack trace.

WHY: the drainer writes the stack as raw return addresses and resolves them with
dbghelp, which reads PDB -- NOT the DWARF debug info that MinGW/GCC emits. So the
system-DLL frames get names but visSele.exe frames stay as bare 0x... addresses.
This tool resolves the visSele.exe frames the reliable way: it reads the matching
minidump (.dmp) to get the ASLR load base, rebases each frame to its preferred
virtual address (ImageBase + RVA), and runs addr2line (DWARF-aware) on a -g build
(RelWithDebInfo or Debug).

USAGE:
  python symbolicate_dump.py <crash_*.dump> [--dmp <insp_crash_*.dmp>]
                             [--exe <visSele.exe>] [--addr2line <addr2line.exe>]

If --dmp/--exe are omitted it auto-discovers: the newest insp_crash_*.dmp in the
dump's directory, and build/win-mingw-msys/visSele.exe under the repo. The exe
MUST be the same build that produced the dump and built with debug info.
"""
import argparse, glob, os, re, struct, subprocess, sys

IMAGE_BASE = 0x140000000  # MinGW x86-64 default PE ImageBase (objdump -p shows it)


def minidump_module_base(dmp_path, want="vissele.exe"):
    data = open(dmp_path, "rb").read()
    if data[:4] != b"MDMP":
        sys.exit(f"{dmp_path}: not a minidump")
    nstreams, dir_rva = struct.unpack_from("<II", data, 8)
    for i in range(nstreams):
        stype, _dsize, rva = struct.unpack_from("<III", data, dir_rva + i * 12)
        if stype != 4:  # ModuleListStream
            continue
        nmod = struct.unpack_from("<I", data, rva)[0]
        off = rva + 4
        for _ in range(nmod):
            base, size = struct.unpack_from("<QI", data, off)       # BaseOfImage u64, SizeOfImage u32
            name_rva = struct.unpack_from("<I", data, off + 20)[0]  # ModuleNameRva
            slen = struct.unpack_from("<I", data, name_rva)[0]
            name = data[name_rva + 4:name_rva + 4 + slen].decode("utf-16le")
            if os.path.basename(name).lower() == want:
                return base, size
            off += 108  # sizeof(MINIDUMP_MODULE)
    sys.exit(f"{dmp_path}: module '{want}' not found")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dump")
    ap.add_argument("--dmp")
    ap.add_argument("--exe")
    ap.add_argument("--addr2line", default="addr2line")
    a = ap.parse_args()

    dump_dir = os.path.dirname(os.path.abspath(a.dump))
    dmp = a.dmp or max(glob.glob(os.path.join(dump_dir, "insp_crash_*.dmp")),
                       key=os.path.getmtime, default=None)
    if not dmp:
        sys.exit("no .dmp given and none found next to the dump")
    exe = a.exe
    if not exe:
        # Walk up from the dump dir looking for build/<preset>/visSele.exe.
        d = dump_dir
        for _ in range(5):
            for c in ("build/win-mingw-msys/visSele.exe", "build/win-mingw/visSele.exe"):
                cand = os.path.join(d, c)
                if os.path.exists(cand):
                    exe = cand; break
            if exe:
                break
            d = os.path.dirname(d)
    if not exe or not os.path.exists(exe):
        sys.exit("visSele.exe not found; pass --exe")

    base, size = minidump_module_base(dmp)
    print(f"[dmp] {os.path.basename(dmp)}  base=0x{base:016x} size=0x{size:x}")
    print(f"[exe] {exe}\n")

    frames = []
    for line in open(a.dump):
        m = re.match(r"#(\d+)\s+0x([0-9a-fA-F]+)", line.strip())
        if not m:
            continue
        idx, addr = int(m.group(1)), int(m.group(2), 16)
        if base <= addr < base + size:
            frames.append((idx, IMAGE_BASE + (addr - base)))

    if not frames:
        sys.exit("no visSele.exe frames in the dump (wrong .dmp pairing?)")
    vas = [f"0x{va:x}" for _, va in frames]
    out = subprocess.run([a.addr2line, "-e", exe, "-f", "-C", "-i", *vas],
                         capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit(f"addr2line failed: {out.stderr}")
    lines = out.stdout.strip().split("\n")
    oi = 0
    for idx, _va in frames:
        func = lines[oi] if oi < len(lines) else "?"
        loc = lines[oi + 1] if oi + 1 < len(lines) else "?"
        oi += 2
        print(f"#{idx:<2} {func}   {loc}")


if __name__ == "__main__":
    main()
