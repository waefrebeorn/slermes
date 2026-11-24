#!/usr/bin/env python3
"""gate_batch.py -- verify K differential gates with the fewest possible spawns.

Measured on this box under load: a bash+python spawn costs 0.3-1.7 s, so
`fast_gates.py` (link spawn + harness spawn + *python spawn per gate*) pays
one interpreter startup per oracle.  This driver runs the same link and
harness steps but executes every oracle **inside its own process** via runpy,
so a 60-gate wave pays 0 interpreter startups instead of 60.

Same contract as fast_gates.py: one PASS/FAIL line per gate, then
`oracles run: N`, exit 0 only when every gate is green.

    C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe scripts/gate_batch.py \
        --plan docs/wave23-plan.json --jobs 8
    C:/.../python.exe scripts/gate_batch.py w23_aio_785f45 w23_core_082b31

`--repeat N` runs the sample N times and prints the per-gate wall time, which
is how a speed claim gets re-measured instead of extrapolated.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import contextlib
import hashlib
import io
import json
import os
import runpy
import shutil
import subprocess
import sys
import tempfile
import time
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fast_gates as fg  # noqa: E402  (same maps, same link, same object sets)

REPO = fg.REPO


def _run_harness(exe, data, env):
    return subprocess.run([exe], input=data, capture_output=True, env=env, cwd=REPO)


def _capture():
    """A text stream that oracles can .reconfigure(), plus a getter for its text."""
    buf = io.BytesIO()
    stream = io.TextIOWrapper(buf, encoding="utf-8", errors="replace", write_through=True)
    return stream, lambda: (stream.flush() or buf.getvalue().decode("utf-8", "replace"))


def _oracle_inproc(gate, harness_stdout: bytes):
    """Run tests/oracle_<gate>.py in this interpreter; return (rc, text).

    Every oracle gets the module table and sys.path it would have had in a fresh
    process: without that, the first oracle to import a name as a *namespace*
    package (hermes_constants resolves that way when the quarry copy is not yet
    first on sys.path) leaves it in sys.modules and every later oracle fails with
    "cannot import name X from hermes_constants (unknown location)". One
    interpreter for all oracles is only safe with per-oracle isolation.
    """
    path = os.path.join(REPO, "tests", "oracle_%s.py" % gate)
    if not os.path.isfile(path):
        return 127, "no tests/oracle_%s.py" % gate
    out, get_out = _capture()
    err, get_err = _capture()
    rc = 0
    old = (sys.stdin, sys.stdout, sys.stderr)
    saved_mods = dict(sys.modules)
    saved_path = list(sys.path)
    try:
        stdin = io.TextIOWrapper(io.BytesIO(harness_stdout), encoding="utf-8", errors="replace")
        sys.stdin, sys.stdout, sys.stderr = stdin, out, err
        runpy.run_path(path, run_name="__main__")
    except SystemExit as exc:
        rc = 0 if exc.code in (None, 0) else (1 if isinstance(exc.code, str) else int(exc.code))
    except BaseException:  # noqa: BLE001 - an oracle crash fails its gate, not the driver
        rc = 1
        err.write(traceback.format_exc())
    finally:
        sys.stdin, sys.stdout, sys.stderr = old
        sys.path[:] = saved_path
        for name in [n for n in sys.modules if n not in saved_mods]:
            del sys.modules[name]
        for name, mod in saved_mods.items():
            if sys.modules.get(name) is not mod:
                sys.modules[name] = mod
    return rc, get_out() + get_err()


def _stamp(gate, objs):
    """A link key: the sources, the objects and the flags that produced the exe."""
    harness = fg.harness_name(gate, fg.ALIAS)
    parts = [fg.GCC, harness]
    for p in ("tests/t_port_%s.c" % harness,) + tuple(objs):
        try:
            st = os.stat(os.path.join(REPO, p))
            parts.append("%s:%d:%d" % (p, st.st_size, int(st.st_mtime)))
        except OSError:
            parts.append("%s:missing" % p)
    parts.extend(fg.DEFS)
    return hashlib.md5("|".join(parts).encode()).hexdigest()


def _link_or_reuse(gate, objs, incs):
    """Reuse the harness exe when nothing it was built from changed.

    Relinking is where a gate run spends its wall clock (one gcc+ld per gate);
    an oracle is milliseconds. A wave gets verified several times (child proof,
    parent gate run, regression re-run), so skipping an unchanged link is the
    difference between a ~30 s and a ~5 s re-verification.
    """
    harness = fg.harness_name(gate, fg.ALIAS)
    exe = os.path.join(fg.HARNESS_DIR, "%s.exe" % harness)
    keyfile = os.path.join(fg.HARNESS_DIR, "%s.key" % harness)
    want = _stamp(gate, objs)
    if os.path.isfile(exe) and os.path.isfile(keyfile):
        try:
            if open(keyfile, encoding="utf-8").read().strip() == want:
                return gate, exe, None
        except OSError:
            pass
    gate, out, err = fg.link(gate, objs, incs)
    if not err:
        try:
            with open(keyfile, "w", encoding="utf-8", newline="\n") as fh:
                fh.write(want + "\n")
        except OSError:
            pass
    return gate, out, err


def verify(gates, jobs):
    """{(gate: PASS/FAIL line)}, in the given order."""
    fg.ALIAS, fg.DEPS = {}, {}
    fg.DEPS, fg.ALIAS = fg.parse_maps()
    fg.INCS = fg.include_args()
    fg.FULL_CACHE = []
    os.makedirs(fg.HARNESS_DIR, exist_ok=True)
    # Native extensions inside the WindowsApps payload fail to load in-process
    # ("DLL load failed ... Access is denied"); the same wheels copied to a normal
    # directory load fine, and PYTHONPATH alone cannot fix an already-running
    # interpreter, so the copy goes on sys.path explicitly.
    if os.path.isdir(fg.SITECOPY) and fg.SITECOPY not in sys.path:
        sys.path.insert(0, fg.SITECOPY)

    missing = set()
    unwired = []
    for gate in gates:
        objs = fg.objects_for(gate, fg.DEPS, fg.FULL_CACHE)
        if gate not in fg.DEPS and not any(o.startswith("src/") for o in objs):
            unwired.append(gate)
        for obj in objs:
            if not obj.startswith("@") and not os.path.isfile(os.path.join(REPO, obj)):
                missing.add(obj)
    if unwired:
        # A gate with no DEPS entry silently links against json.o alone and dies
        # with "collect2 ... undefined reference" to its own port symbols. Say
        # what is actually wrong: the gate is not wired.
        print("NOT WIRED (no DEPS entry): %s -- run scripts/wave_land.py wire"
              % " ".join(unwired), file=sys.stderr)
    if missing:
        subprocess.run(["make", "-j%d" % jobs] + sorted(missing), cwd=REPO,
                       capture_output=True, text=True, errors="replace")

    lines = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        links = list(pool.map(
            lambda g: _link_or_reuse(g, fg.objects_for(g, fg.DEPS, fg.FULL_CACHE), fg.INCS),
            gates))

    runs = []
    for gate, exe, err in links:
        if err:
            lines[gate] = "FAIL %s  %s" % (gate, err)
            continue
        fixture = fg.fixture_for(gate, fg.harness_name(gate, fg.ALIAS))
        data = open(os.path.join(REPO, fixture), "rb").read() if fixture else b""
        tmp = tempfile.mkdtemp(prefix="gb_", dir=os.path.join(REPO, ".testbin"))
        env = dict(os.environ, SLERMES_HOME=tmp, HOME=tmp, PYTHONIOENCODING="utf-8")
        if os.path.isdir(fg.SITECOPY):
            env["PYTHONPATH"] = fg.SITECOPY + (os.pathsep + env["PYTHONPATH"]
                                               if env.get("PYTHONPATH") else "")
        runs.append((gate, exe, data, env, tmp))

    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
            results = list(pool.map(
                lambda t: (t[0], subprocess.run([t[1]], input=t[2], capture_output=True,
                                                env=t[3], cwd=REPO)), runs))
    finally:
        for _g, _e, _d, _env, tmp in runs:
            shutil.rmtree(tmp, ignore_errors=True)

    payload = {}
    for gate, h in results:
        if h.returncode != 0:
            lines[gate] = "FAIL %s  rc=%d  harness crashed (partial output is not a pass)" % (
                gate, h.returncode)
        else:
            payload[gate] = h.stdout
    for gate in gates:
        if gate in lines:
            continue
        rc, text = _oracle_inproc(gate, payload.get(gate, b""))
        line = ""
        for cand in text.splitlines():
            if cand.startswith("ORACLE %s:" % gate):
                line = cand.strip()
        if rc == 0 and line:
            lines[gate] = "PASS %s  (%s)" % (gate, line.split(": ", 1)[1])
        else:
            detail = line or (text.strip().splitlines()[-1][:200] if text.strip() else "no oracle output")
            lines[gate] = "FAIL %s  rc=%d  %s" % (gate, rc, detail)
    return [lines[g] for g in gates]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gates", nargs="*")
    ap.add_argument("--plan")
    ap.add_argument("--jobs", type=int, default=fg.JOBS)
    ap.add_argument("--repeat", type=int, default=1)
    args = ap.parse_args()

    gates = list(args.gates)
    if args.plan:
        gates += list(json.load(open(args.plan, encoding="utf-8")))
    if not gates:
        print("no gates given", file=sys.stderr)
        return 2

    # link once, reuse across --repeat rounds so the repeat measures steady state
    bad = 0
    times = []
    for _ in range(args.repeat):
        t0 = time.perf_counter()
        out = verify(gates, args.jobs)
        times.append(time.perf_counter() - t0)
        bad = 0
        for line in out:
            print(line)
            if not line.startswith("PASS "):
                bad += 1
        print("----")
        print("oracles run: %d  (%s)" % (len(gates),
              "exit 0 = all green, no drift" if not bad else "NOT all green: %d failed" % bad))
    if args.repeat > 1:
        print("wall: %s  best %.2fs" % (" ".join("%.2fs" % t for t in times), min(times)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
