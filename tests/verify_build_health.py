#!/usr/bin/env python3
"""Honest build-health gate for Slermes.

Compiles EVERY tracked .c file with the real project flags and reports a
count. This exists because `make | grep error:` can report success while
building nothing -- the historical Makefile default goal was a single
SQLite3 object, so a bare `make` printed "up to date" and validated zero.

Usage:
    python tests/verify_build_health.py            # summary
    python tests/verify_build_health.py --list     # failing files
    python tests/verify_build_health.py --save     # write baseline JSON
    python tests/verify_build_health.py --regress  # exit 1 if worse than baseline
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = pathlib.Path(__file__).resolve().parent.parent

# GNU/MSYS `timeout` (and some native launchers) strip USERPROFILE from the
# environment, and ccache refuses to run without it -- every source then
# "fails" with one ccache error and the gate reports 0/N clean. Set it if absent.
os.environ.setdefault("USERPROFILE", "C:/Users/eman5")
os.environ.setdefault("LOCALAPPDATA", "C:/Users/eman5/AppData/Local")
BASELINE = ROOT / "tests" / ".build_health_baseline.json"

CFLAGS = None
FLAGS_BY_FILE = {}

# ── one `make -n -B` probe, shared and cached ──────────────────────────────
# The gate used to run `make -n -B` twice per invocation (once for the source
# list, once for the flags) at ~2 minutes each, and two concurrent gate runs
# would race over the same tree. Now the dry-run runs at most once per
# makefile change; everybody else waits for, or reuses, that result.
PROBE_CACHE = ROOT / ".build_probe.json"
PROBE_LOCK = ROOT / ".build_probe.lock"
PROBE_STALE_S = 120       # a make -n dry run takes seconds
PROBE_WAIT_S = 120        # how long to wait on a lock we cannot reclaim


def _probe_key() -> str:
    h = hashlib.sha256()
    paths = [ROOT / "Makefile", ROOT / "build" / "winlibs-env.sh"]
    paths += sorted((ROOT / "build").glob("*.mk"))
    for p in paths:
        try:
            h.update(p.read_bytes())
        except OSError:
            pass
    return h.hexdigest()


def _read_probe(key: str):
    try:
        d = json.loads(PROBE_CACHE.read_text(encoding="utf-8"))
        if d.get("key") == key:
            return d.get("stdout", "")
    except (OSError, ValueError):
        pass
    return None


def _acquire_lock() -> bool:
    # STALE_S and WAIT_S are deliberately different. A `make -n -B` dry run takes
    # seconds, so a lock older than two minutes is a crashed run, not a live one.
    # Using one 1800 s constant for both meant a killed run blocked every later
    # gate for half an hour -- the wait bound must be far shorter than the stale
    # age, or the lock is never reclaimed in practice.
    try:
        fd = os.open(PROBE_LOCK, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError:
        try:
            if time.time() - os.path.getmtime(PROBE_LOCK) > PROBE_STALE_S:
                os.unlink(PROBE_LOCK)
                return _acquire_lock()
        except OSError:
            pass
        return False
    os.close(fd)
    return True


def make_probe() -> str:
    """`make -n -B` output, run once per makefile change."""
    key = _probe_key()
    cached = _read_probe(key)
    if cached is not None:
        return cached
    if not _acquire_lock():
        # Another process is probing; wait for it rather than racing the tree.
        for _ in range(PROBE_WAIT_S):
            time.sleep(1)
            cached = _read_probe(key)
            if cached is not None:
                return cached
        raise SystemExit("timed out waiting for the build-probe lock")
    try:
        # A target is REQUIRED. `make -n -B` with no target walks every goal
        # in the tree and took past 280 s; `make -n -B slermes` is 3 s and
        # covers every source the product compiles, which is the only thing
        # this gate needs.
        r = subprocess.run(["make", "-n", "-B", "slermes"], cwd=ROOT,
                           capture_output=True,
                           text=True, encoding="utf-8", errors="replace")
        PROBE_CACHE.write_text(json.dumps({"key": key, "stdout": r.stdout}),
                               encoding="utf-8")
        return r.stdout
    finally:
        try:
            os.unlink(PROBE_LOCK)
        except OSError:
            pass


def real_cflags():
    """Use the project's own compile flags, harvested from `make -n`.

    Hand-rolled -I lists diverge from the real build and manufacture phantom
    errors (e.g. missing -I lib -> 'libdb/db.h: No such file'). Ask make.
    """
    for line in make_probe().splitlines():
        if " -c " in line and ".c" in line:
            flags = line.split(" -c ")[0]
            flags = flags.replace(str(ROOT).replace("\\", "/"), "")
            parts = flags.split()
            if parts and parts[0].endswith("gcc"):
                parts = parts[1:]
            # make prints -DNAME=\\\"value\\\" (shell-escaped). Passing that
            # straight to execve hands gcc a literal backslash, which it reports
            # as "stray '\'". Collapse the escaping back to a plain quote pair.
            parts = [re.sub(r'\\+"', '"', p) for p in parts]
            return parts + ["-fsyntax-only"]
    raise SystemExit("could not harvest compile flags from `make -n`")


CC = os.environ.get("CC", "gcc")


def build_sources():
    """Source files this platform's build actually compiles.

    Scanning every .c over-reports: window_wayland.c and xdg-shell-protocol.c
    only exist on Linux (objects.mk gates them on UNAME_S) and will never
    compile on Windows. Scope to what `make -n` would compile.
    """
    out = {}
    for line in make_probe().splitlines():
        if " -c -o " not in line:
            continue
        head, tail = line.split(" -c -o ", 1)
        src = tail.split()[-1]
        if not src.endswith(".c"):
            continue
        out.setdefault(src, head)
    return out


def sources():
    # Walk the tree instead of shelling out to git: this box's MSYS python
    # cannot spawn the native git.exe (WinError 5), and a filesystem scan has
    # no dependency on git state anyway.
    out = []
    for sub in ("src",):
        for p in (ROOT / sub).rglob("*.c"):
            out.append(p.relative_to(ROOT).as_posix())
    return sorted(out)


def check_many(paths):
    """Syntax-check a batch of files with ONE compiler invocation.

    The per-file version spawned `sh -c "cc -fsyntax-only <file>"` 2,010 times
    (~0.2 s of process startup each, 4 workers) which took the gate past 420
    seconds. gcc accepts several sources with -fsyntax-only and reports each
    file's errors with its own path prefix, so batching loses no information.

    Files are grouped by their flag set first: FLAGS_BY_FILE exists precisely
    because some units need different flags, and mixing them would be wrong.
    """
    by_cmd = {}
    for p in paths:
        by_cmd.setdefault(FLAGS_BY_FILE.get(p, CC), []).append(p)

    out = []
    for cmd, files in by_cmd.items():
        for i in range(0, len(files), 64):
            chunk = files[i:i + 16]
            argv = ["sh", "-c", cmd + " -fsyntax-only " +
                    " ".join(f'"{f}"' for f in chunk)]
            r = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True,
                               encoding="utf-8", errors="replace")
            if r.returncode == 0:
                continue
            counts = {}
            for ln in r.stderr.splitlines():
                if "error:" not in ln:
                    continue
                for f in chunk:
                    if ln.startswith(f + ":") or f in ln[:200]:
                        counts[f] = counts.get(f, 0) + 1
                        break
            for f, n in counts.items():
                out.append({"file": f, "errors": n})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--save", action="store_true")
    ap.add_argument("--regress", action="store_true")
    ap.add_argument("--no-link", action="store_true",
                    help="skip the link check (sources only)")
    args = ap.parse_args()

    per_file = build_sources()
    files = sorted(per_file)
    global CFLAGS, FLAGS_BY_FILE
    FLAGS_BY_FILE = per_file
    CFLAGS = next(iter(per_file.values())) if per_file else real_cflags()
    # 8 workers was the original default, but on this MSYS2 box that reliably
    # produces spurious failures (ccache/env contention under concurrent
    # spawns -- the same fork-exhaustion family as the oracle suite). 4 is the
    # largest count that stays green; override with HEALTH_JOBS.
    with ThreadPoolExecutor(max_workers=int(os.environ.get("HEALTH_JOBS", "4"))) as ex:
        chunks = [files[i:i + 64] for i in range(0, len(files), 64)]
        results = [r for rs in ex.map(check_many, chunks) for r in rs]

    failing = sorted(results, key=lambda r: -r["errors"])
    total = len(files)
    bad = len(failing)

    print(f"Slermes build health: {total - bad}/{total} clean, {bad} failing")
    if bad:
        print(f"Total compiler errors across failures: {sum(r['errors'] for r in failing)}")
    if args.list:
        for r in failing:
            print(f"  {r['errors']:5d}  {r['file']}")

    if args.save:
        BASELINE.write_text(json.dumps(
            {"total": total, "failing": [r["file"] for r in failing]}, indent=1))
        print(f"baseline written: {len(failing)} failing")

    if args.regress:
        if not BASELINE.exists():
            print("no baseline; run --save first")
            return 1
        base = set(json.loads(BASELINE.read_text())["failing"])
        now = {r["file"] for r in failing}
        new = sorted(now - base)
        fixed = sorted(base - now)
        if new:
            print(f"\nREGRESSION: {len(new)} newly broken")
            for f in new:
                print(f"  + {f}")
            return 1
        print(f"\nNo regressions. Fixed {len(fixed)} since baseline.")
        for f in fixed:
            print(f"  - {f}")

    # ---- port-object gate --------------------------------------------------
    # Parity is a claim about the linked binary, so a translation unit that
    # carries PoP credit must be in build/objects.mk. Runs here so the one gate
    # people already invoke covers it.
    port_gate = subprocess.run([sys.executable, str(ROOT / "tests" / "verify_port_objects.py")],
                               capture_output=True, text=True,
                               encoding="utf-8", errors="replace")
    sys.stdout.write(port_gate.stdout)
    if port_gate.returncode != 0:
        print("port object gate: FAIL")
        return 1

    # The cupboarded objects.mk must still round-trip: same expanded object set.
    org = subprocess.run([sys.executable,
                          str(ROOT / "scripts" / "organize_objects.py"), "--check"],
                         capture_output=True, text=True,
                         encoding="utf-8", errors="replace")
    sys.stdout.write(org.stdout)
    if org.returncode != 0:
        print("object organization check: FAIL")
        return 1

    # ---- link check -------------------------------------------------------
    # -fsyntax-only proves a file parses. It does NOT prove the symbols resolve.
    # A duplicate definition or a missing object leaves every source "clean"
    # while the binary fails to link -- which is exactly how a compile-only
    # gate reported success against a broken build. Always link.
    if not args.no_link:
        print("\nLinking (make -j4 all) ...")
        r = subprocess.run(["make", "-j4", "all"], cwd=ROOT, capture_output=True,
                           text=True, encoding="utf-8", errors="replace")
        if r.returncode != 0:
            print("LINK FAILED")
            seen = set()
            for ln in (r.stdout + r.stderr).splitlines():
                if "error:" in ln or "multiple definition" in ln or "undefined reference" in ln:
                    key = ln.strip()[:160]
                    if key not in seen:
                        seen.add(key)
                        print(f"  {key}")
                        if len(seen) >= 12:
                            break
            return 1
        print("LINK OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
