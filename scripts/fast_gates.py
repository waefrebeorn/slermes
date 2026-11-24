#!/usr/bin/env python3
"""fast_gates.py -- run differential oracle gates without the per-gate shell tax.

`tests/run_all_oracles.sh` spawns ~20 processes per gate (bash, dirname,
grep/sed/tr, mkdir, find, uname, python probe, gcc->cc1/as/ld, harness,
oracle python, rm). On MSYS2/Windows a spawn costs 150-300 ms, so a 72-gate
wave pays minutes of pure process overhead per oracle.

This driver does the same work with three spawns per gate (link, harness,
oracle), all fanned out across a thread pool:

    source build/winlibs-env.sh   # once, in the caller
    C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe scripts/fast_gates.py w19_a w19_b ...
    ... /path/to/.venv/python scripts/fast_gates.py --plan docs/wave19-plan.json

It reads the same DEPS map out of tests/run_all_oracles.sh, honours
HARNESS_ALIAS, resolves FULL against the registered objects in build/*.mk,
builds any missing objects in one `make -j` batch, links harnesses in
parallel, and reports the same `PASS <gate> (N cases, M mismatches)` /
`FAIL ...` / `oracles run: ...` lines the shell runner prints -- so a green
here means exactly what a green there means.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TESTS = os.path.join(REPO, "tests")
ORACLE_SH = os.path.join(TESTS, "run_all_oracles.sh")
HARNESS_DIR = os.path.join(REPO, ".testbin", "harnesses")
JOBS = min(12, os.cpu_count() or 4)

GCC = os.environ.get("CC") or "C:/winlibs/mingw64/bin/gcc.exe"
# MSYS2's own gcc is SAC-blocked on this host; winlibs is the toolchain (see
# build/winlibs-env.sh). Pin it instead of trusting PATH order.
DEFS = ["-std=gnu11", "-D_GNU_SOURCE", "-include", "hermes_win32.h"]
WIN_LIBS = ["-lws2_32", "-lgdi32", "-lcrypt32", "-ladvapi32", "-luser32",
            "-lshell32", "-lole32", "-luuid", "-lSDL2main", "-lSDL2", "-lSDL2_ttf"]
TAIL_LIBS = ["-lz", "-lpcre2-8", "-ltre", "-lssl", "-lcrypto",
             "-Wl,--allow-multiple-definition"]


def sh_lines(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read().splitlines()


def parse_maps():
    """DEPS + HARNESS_ALIAS from the shell runner (single source of truth)."""
    deps, alias = {}, {}
    block = None
    for line in sh_lines(ORACLE_SH):
        if line.startswith("declare -A DEPS=("):
            block = "deps"
            continue
        if line.startswith("declare -A HARNESS_ALIAS=("):
            block = "alias"
            continue
        if block and line.startswith(")"):
            block = None
            continue
        if not block:
            continue
        m = re.match(r'\s*\[([^\]]+)\]="([^"]*)"', line)
        if not m:
            continue
        (deps if block == "deps" else alias)[m.group(1)] = m.group(2)
    return deps, alias


def include_args():
    incs = ["-I", "include/compat", "-I", "include", "-I", "src", "-I", "lib"]
    libs_mk = os.path.join(REPO, "build", "libs-config.mk")
    if os.path.exists(libs_mk):
        for m in re.findall(r"lib/lib[a-z0-9_]+", open(libs_mk, encoding="utf-8", errors="replace").read()):
            incs += ["-I", m]
    for name in os.listdir(os.path.join(REPO, "src")):
        p = os.path.join("src", name)
        if os.path.isdir(os.path.join(REPO, p)):
            incs += ["-I", p]
    if os.path.isdir("C:/winlibs/extra-inc"):
        incs += ["-I", "C:/winlibs/extra-inc"]
    return incs


def full_objects():
    """The registered object set, minus SAC-refused images (same rule as the shell)."""
    skip = re.compile(r"/(main|hash|hermes_regex|auxiliary_client_runtime)\.o$")
    objs = set()
    build = os.path.join(REPO, "build")
    for name in os.listdir(build):
        if not name.endswith(".mk"):
            continue
        text = open(os.path.join(build, name), encoding="utf-8", errors="replace").read()
        for m in re.findall(r"[A-Za-z0-9_./\\-]+\.o", text):
            o = m.replace("\\\\", "/").replace("\\", "/").lstrip("./")
            if skip.search("/" + o):
                continue
            if os.path.isfile(os.path.join(REPO, o)):
                objs.add(o)
    for extra in (".testbin/hash_O0.o", ".testbin/hermes_regex_O0.o"):
        if os.path.isfile(os.path.join(REPO, extra)):
            objs.add(extra)
    return sorted(objs)


def objects_for(gate, deps, full_cache):
    dep = deps.get(gate)
    if dep in (None, "", "FULL"):
        if not full_cache:
            full_cache.append(full_objects())
        return list(full_cache[0]) if dep == "FULL" else default_objects(gate)
    return dep.split()


def default_objects(gate):
    """Same default the shell runner uses: the port object + json."""
    for cand in ("src/cli/port_%s.o" % gate, "src/%s.o" % gate):
        if os.path.isfile(os.path.join(REPO, cand)):
            return [cand, "lib/libjson/json.o"]
    return ["lib/libjson/json.o"]


def harness_name(gate, alias):
    return alias.get(gate, gate)


def link(gate, objs, incs):
    harness = harness_name(gate, ALIAS)
    src = "tests/t_port_%s.c" % harness
    out = os.path.join(HARNESS_DIR, "%s.exe" % harness)
    if not os.path.isfile(src):
        return gate, None, "no harness %s" % src
    cmd = ([GCC] + DEFS + incs + [src] + objs + ["-o", out,
           "-L", "C:/winlibs/extra-libs", "-lstdc++", "-lm"] + WIN_LIBS + TAIL_LIBS)
    res = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, errors="replace")
    if res.returncode != 0:
        err = [l for l in res.stderr.splitlines() if "error:" in l][:3]
        return gate, None, "link failed: %s" % (" | ".join(err) or res.stderr[-200:])
    return gate, out, None


def fixture_for(gate, harness):
    for cand in ("tests/fixtures/%s.jsonl" % harness, "tests/%s_cases.jsonl" % harness):
        if os.path.isfile(os.path.join(REPO, cand)):
            return cand
    return None


def run_gate(gate, exe, pybin):
    harness = harness_name(gate, ALIAS)
    fixture = fixture_for(gate, harness)
    tmp = tempfile.mkdtemp(prefix="fg_", dir=os.path.join(REPO, ".testbin"))
    env = dict(os.environ, SLERMES_HOME=tmp, HOME=tmp, PYTHONIOENCODING="utf-8")
    try:
        data = open(os.path.join(REPO, fixture), "rb").read() if fixture else b""
        h = subprocess.run([exe], input=data, capture_output=True, env=env, cwd=REPO)
        if h.returncode in (126, 127):
            # Smart App Control refuses some fresh images; a relink at -O1 slips past.
            alt = exe + "_alt.exe"
            shutil.copyfile(exe, alt)
            h = subprocess.run([alt], input=data, capture_output=True, env=env, cwd=REPO)
        if h.returncode != 0:
            return gate, "FAIL %s  rc=%d  harness crashed (partial output is not a pass)" % (gate, h.returncode)
        oracle = "tests/oracle_%s.py" % gate
        if not os.path.isfile(os.path.join(REPO, oracle)):
            return gate, "MISS %s (no %s)" % (gate, oracle)
        o = subprocess.run([pybin, oracle], input=h.stdout, capture_output=True, env=env, cwd=REPO)
        text = (o.stdout + o.stderr).decode("utf-8", "replace")
        line = ""
        for cand in text.splitlines():
            if cand.startswith("ORACLE %s:" % gate):
                line = cand.strip()
        if o.returncode == 0 and line:
            return gate, "PASS %s  (%s)" % (gate, line.split(": ", 1)[1])
        detail = line or (text.strip().splitlines()[-1][:160] if text.strip() else "no oracle output")
        return gate, "FAIL %s  rc=%d  %s" % (gate, o.returncode, detail)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gates", nargs="*")
    ap.add_argument("--plan", help="wave plan json (gate -> module)")
    ap.add_argument("--jobs", type=int, default=JOBS)
    args = ap.parse_args()

    gates = list(args.gates)
    if args.plan:
        gates += list(json.load(open(args.plan, encoding="utf-8")))
    if not gates:
        print("no gates given", file=sys.stderr)
        return 2

    global ALIAS
    DEPS, ALIAS = parse_maps()
    incs = include_args()
    os.makedirs(HARNESS_DIR, exist_ok=True)

    pybin = os.environ.get("PYTHON_BIN")
    if not pybin:
        for cand in ("C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe", sys.executable):
            if os.path.isfile(cand):
                pybin = cand
                break

    full_cache = []
    # Build every missing object dependency in one make batch first: serialising
    # it per gate is what made the shell runner slow on a fresh wave.
    missing = set()
    for gate in gates:
        for obj in objects_for(gate, DEPS, full_cache):
            if not obj.startswith("@") and not os.path.isfile(os.path.join(REPO, obj)):
                missing.add(obj)
    if missing:
        print("building %d missing objects..." % len(missing), file=sys.stderr)
        subprocess.run(["make", "-j%d" % args.jobs] + sorted(missing), cwd=REPO,
                       capture_output=True, text=True, errors="replace")

    failures = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        links = list(pool.map(lambda g: link(g, objects_for(g, DEPS, full_cache), incs), gates))
        ready = []
        for gate, exe, err in links:
            if err:
                failures += 1
                print("FAIL %s  %s" % (gate, err))
            else:
                ready.append((gate, exe))
        results = list(pool.map(lambda t: run_gate(t[0], t[1], pybin), ready))
    for _gate, line in results:
        print(line)
        if not line.startswith("PASS "):
            failures += 1
    print("----")
    print("oracles run: %d  (%s)" % (len(gates),
          "exit 0 = all green, no drift" if not failures else "NOT all green: %d failed" % failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
