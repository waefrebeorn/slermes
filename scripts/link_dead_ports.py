#!/usr/bin/env python3
"""Link every unlinked port-style twin that links without a collision.

Parity is a claim about the binary. 165 port-style .c files carry PoP credit but
are absent from build/objects.mk, so their features read as missing even though
the C exists. Most define symbols the binary has never heard of and can simply
be linked.

A twin is rejected only when linking it would change the program:
 - it defines a symbol the binary already defines (multiple definition), or
 - it needs a symbol nothing can provide (undefined reference).

Compilation goes through `make <obj>` targets, which is fast (the project's own
flags, no hand-rolled -I list, and no `make -n -B` -- that dry-run of all 1,127
objects takes minutes and is pure overhead here).

Writes the survivors to build/ports_linked.mk as CUPBOARD_* additions.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
MK = REPO / "build" / "objects.mk"
OUT = REPO / "build" / "ports_linked.mk"
BIN = REPO / "slermes.exe"
# Native Windows form: Python does not translate MSYS '/mingw64/...' paths.
NM = "C:/msys64/mingw64/bin/nm.exe"


def is_port_style(rel: str) -> bool:
    return '/ports/' in rel or Path(rel).name.startswith('port_') \
        or rel.endswith('_ports.c')


def symbols(obj: Path, kind: str) -> set[str]:
    """kind='def' for defined, 'undef' for undefined.

    Section symbols (.text/.bss/.data/.rdata/...) are NOT definitions -- they
    appear in every object and would make every twin look like a collision.
    """
    r = subprocess.run([NM, str(obj)], capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    out = set()
    for ln in r.stdout.splitlines():
        parts = ln.split()
        if kind == "def" and len(parts) >= 3 and parts[1] in "TtDdBbRrSs":
            name = parts[2]
            if not name.startswith("."):
                out.add(name)
        elif kind == "undef" and parts and parts[0] == "U":
            out.add(parts[-1])
    return out


def main() -> int:
    text = MK.read_text(encoding="utf-8", errors="replace")
    linked = set(re.findall(r'(src/[A-Za-z0-9_/]+\.o)', text))

    cands = []
    for c in sorted((REPO / "src").rglob("*.c")):
        rel = c.relative_to(REPO).as_posix()
        obj = c.with_suffix('.o').relative_to(REPO).as_posix()
        if obj in linked or not is_port_style(rel):
            continue
        try:
            if not re.search(r'PoP:', c.read_text(encoding="utf-8", errors="replace")):
                continue
        except OSError:
            continue
        cands.append(obj)

    print(f"candidates: {len(cands)}")
    # Native Windows form: Python does not translate MSYS '/c/...' paths.
    make = next((p for p in ("C:/msys64/usr/bin/make.exe",
                             "C:/msys64/mingw64/bin/make.exe",
                             "make")
                 if Path(p).exists() or p == "make"), "make")
    r = subprocess.run([make, "-j8", *cands], cwd=REPO,
                       capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    compiled = [o for o in cands if (REPO / o).exists()]
    print(f"compiled: {len(compiled)}/{len(cands)} (make rc={r.returncode})")
    if len(compiled) != len(cands):
        errs = [ln for ln in (r.stdout + r.stderr).splitlines()
                if "error:" in ln or "No rule" in ln or "Nothing to be done" in ln]
        for ln in errs[:8]:
            print(f"  {ln.strip()[:150]}")

    bin_syms = symbols(BIN, "def")
    print(f"binary defines {len(bin_syms)} symbols")

    survivors, dropped = [], []
    for o in compiled:
        defs = symbols(REPO / o, "def")
        clash = sorted(defs & bin_syms)
        if clash:
            dropped.append((o, "collides: " + ", ".join(clash[:3])))
        else:
            survivors.append((o, defs))

    available = set(bin_syms)
    for _, defs in survivors:
        available |= defs
    final = []
    for o, defs in survivors:
        missing = sorted(u for u in symbols(REPO / o, "undef") if u not in available)
        if missing:
            dropped.append((o, "undefined: " + ", ".join(missing[:3])))
        else:
            final.append(o)

    print(f"\nsurvivors: {len(final)}   dropped: {len(dropped)}")
    for o, why in dropped[:15]:
        print(f"  DROP {o}: {why}")
    if len(dropped) > 15:
        print(f"  ... and {len(dropped) - 15} more")

    # Prediction is not proof: `nm` on an executable omits non-exported symbols,
    # so a twin can pass the undefined check and still fail to link. Try the
    # real link and drop exactly what the linker rejects.
    if final:
        mk_text = MK.read_text(encoding="utf-8")

        def with_trial(objs):
            # Append to the real PORT_OBJ reference so the trial links them.
            return mk_text.replace("$(PORTS_LINKED_OBJ)",
                                   "$(PORTS_LINKED_OBJ) " + " ".join(objs))

        MK.write_text(with_trial(final), encoding="utf-8", newline="\n")
        try:
            for _ in range(6):
                r = subprocess.run([make, "-j8", "slermes"], cwd=REPO,
                                   capture_output=True, text=True,
                                   encoding="utf-8", errors="replace")
                if r.returncode == 0:
                    break
                rej = set()
                for ln in (r.stdout + r.stderr).splitlines():
                    m = re.search(r"undefined reference to `([^']+)'", ln)
                    if m:
                        for o in final:
                            if m.group(1) in symbols(REPO / o, "def"):
                                rej.add(o)
                    m = re.search(r"multiple definition of `([^']+)'", ln)
                    if m:
                        for o in final:
                            if m.group(1) in symbols(REPO / o, "def"):
                                rej.add(o)
                rej = {o for o in rej if o in final}
                if not rej:
                    break
                for o in rej:
                    dropped.append((o, "linker rejected"))
                final = [o for o in final if o not in rej]
                MK.write_text(with_trial(final), encoding="utf-8", newline="\n")
        finally:
            MK.write_text(mk_text, encoding="utf-8", newline="\n")
        print(f"after trial link: {len(final)} survivors")

    OUT.write_text(
        "# Auto-generated by scripts/link_dead_ports.py on " + sys.platform
        + " -- do not hand-edit.\n"
        "# Dead port-style twins that compile AND link without a collision here.\n"
        "# On another platform re-run:  python scripts/link_dead_ports.py\n"
        "# POSIX-only twins (pthread_timedjoin_np, dirent d_type, sys/syscall.h)\n"
        "# compile on Linux/macOS and are picked up there automatically.\n"
        "PORTS_LINKED_OBJ = " + " ".join(final) + "\n",
        encoding="utf-8", newline="\n")
    print(f"\nwrote {OUT.relative_to(REPO)} ({sys.platform})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
