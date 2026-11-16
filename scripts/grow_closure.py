#!/usr/bin/env python3
"""Grow CUPBOARD_UNBUILT_MODS until a harness links.

A harness can only link if every first-party symbol it references has an object
in the build. Sources that no makefile variable mentions are invisible to
`make print-objs`, so the minimal closure cannot find them and the link fails
with undefined references. Adding them by hand one at a time is slow and
repeats the same discovery every run.

This parses the linker's own undefined-reference list, finds a first-party .c
defining each symbol, adds its object to CUPBOARD_UNBUILT_MODS, builds it, and
repeats until the link is clean or no defining source exists.

Usage: grow_closure.py <port> [max-iterations]
"""
import re
import subprocess
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
MK = REPO / "build/objects.mk"
UNDEF = re.compile(r"undefined reference to `([A-Za-z_]\w*)'")
DEF = re.compile(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\(([^;{]*)\)\s*\{", re.M)


def definitions():
    out = {}
    for base in ("src", "lib"):
        for f in (REPO / base).rglob("*.c"):
            try:
                txt = f.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for m in DEF.finditer(txt):
                out.setdefault(m.group(1), f)
    return out


def add_object(src):
    rel = src.relative_to(REPO).as_posix()
    obj = str(Path(rel).with_suffix(".o")).replace("\\", "/")
    lines = MK.read_text(encoding="utf-8").split("\n")
    start = next(i for i, l in enumerate(lines) if l.startswith("CUPBOARD_UNBUILT_MODS"))
    end = start
    while end + 1 < len(lines) and lines[end].rstrip().endswith("\\"):
        end += 1
    if obj in {l.strip() for l in lines[start:end + 1]}:
        return False
    lines.insert(end, f"    {obj} \\")
    MK.write_text("\n".join(lines), encoding="utf-8")
    return True


def main():
    port = sys.argv[1]
    max_it = int(sys.argv[2]) if len(sys.argv) > 2 else 12
    defs = definitions()
    for it in range(1, max_it + 1):
        log = REPO / f"tests/oracle/build/oracle_link_{port}.log"
        log.unlink(missing_ok=True)
        import os
        env = dict(os.environ)
        env.update({"USERPROFILE": r"C:\Users\eman5",
                    "LOCALAPPDATA": r"C:\Users\eman5\AppData\Local",
                    "PYTHONIOENCODING": "utf-8",
                    "PATH": "/usr/bin:/c/msys64/mingw64/bin:" + os.environ.get("PATH", "")})
        subprocess.run(["bash", "tests/oracle/runners/run_oracle.sh", port],
                       cwd=REPO, env=env, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        # A missing log is NOT a clean link: the runner may have failed before
        # reaching the link at all. Treating it as success is a false pass.
        if not log.exists():
            print(f"iter {it}: no link log written -- the runner never reached the link")
            return 1
        und = sorted(set(UNDEF.findall(text)))
        if not und:
            print(f"iter {it}: no undefined references -- link is clean")
            return 0
        added = []
        missing = []
        for sym in und:
            src = defs.get(sym)
            if src is None:
                missing.append(sym)
                continue
            if add_object(src):
                added.append((sym, src.relative_to(REPO).as_posix()))
        if added:
            subprocess.run(["bash", "-c", ". build/winlibs-env.sh >/dev/null 2>&1; "
                            f"make -s --no-print-directory print-objs > tests/oracle/build/objs.txt 2>/dev/null; "
                            "rm -f tests/oracle/build/sym_index.json; "
                            "python tests/oracle/runners/sym_index.py build @tests/oracle/build/objs.txt >/dev/null 2>&1"],
                           cwd=REPO, shell=True)
            objs = " ".join(o for _, o in added)
            subprocess.run(["bash", "-c", f". build/winlibs-env.sh >/dev/null 2>&1; make -j8 {objs} >/dev/null 2>&1"],
                           cwd=REPO, shell=True)
        print(f"iter {it}: {len(und)} undefined; added {len(added)} object(s); "
              f"{len(missing)} with no first-party definition")
        for sym, o in added:
            print(f"    {sym} <- {o}")
        if not added:
            print("    unresolved:", missing[:8])
            return 1
    return 1


if __name__ == "__main__":
    sys.exit(main())
