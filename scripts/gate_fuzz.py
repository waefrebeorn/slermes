#!/usr/bin/env python3
"""Held-out differential fuzzing for wave gates (parent-side audit).

Why this exists: every gate's fixture is authored by the same agent that wrote the
C port, so the fixture tests the port on inputs its author already had in mind.
The C-to-Rust re-testing study (arXiv 2611.23839) found 160 defects in released
translations precisely because each translator's own check never compared the two
sides on an input that would expose the difference; harnesses "catch 20%-91% of
planted bugs". This tool generates inputs the author never wrote.

Method
------
1. Read tests/fixtures/<gate>.jsonl, keeping only func/case/seams and the inputs.
2. Emit K perturbed variants per case: numeric neighbours, empty/longer/short
   strings, bool flips, null swaps, list truncation/duplication, dict key
   addition, and cross-case value transplants for the same key.
3. Drop "out" from each variant and feed the file to the gate's C harness, which
   computes the port's answer and echoes the inputs.
4. Pipe that filled file to the gate's oracle, which computes the live Python
   answer from the SAME line and compares. Any disagreement is a held-out
   divergence: a real defect or an overfit fixture.

Usage
-----
    python scripts/gate_fuzz.py <gate> [<gate> ...] [--per-case 6] [--jobs 6]
    python scripts/gate_fuzz.py --plan docs/wave32-plan.json --jobs 6

Exit code 0 only when every gate produced zero divergences AND at least
--min-cases comparisons (a vacuous run is a failure, not a pass).
"""
import argparse
import copy
import json
import os
import random
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

REPO = os.getcwd()
PY = sys.executable
BASH = r"C:\msys64\usr\bin\bash.exe" if os.name == "nt" else "sh"
KEEP = {"func", "case", "out", "seams"}
GATE_TIMEOUT = 120


def args_timeout():
    return GATE_TIMEOUT


# Keys that restate a fact another key already carries (a row-count next to the
# row list, an index next to the array). Perturbing one without the other models
# a state the live code can never see: fixture {"names": [...3...], "found": 6}
# with found pushed to 2 reports "missing triggers" for a list of three.
DERIVED_KEYS = {"found", "count", "total", "n", "num", "size", "len", "length"}


def is_discriminator(key):
    """Keys that select WHICH case is being modelled, not the data for it.

    Perturbing `value_type` from "int" to "intx" does not test the port: the
    oracle maps the discriminator onto a different Python object, so the
    divergence is the harness talking to itself. Same for `kind`.
    """
    k = key.lower()
    return k == "kind" or k.endswith("_type") or k.endswith("_kind") or k in DERIVED_KEYS


def harness_exe(gate):
    for cand in (os.path.join(REPO, ".testbin", "harnesses", gate + ".exe"),
                 os.path.join(REPO, ".testbin", "o_" + gate + ".exe")):
        if os.path.isfile(cand):
            return cand
    return None


def build_harness(gate, module, jobs=1):
    """Compile the gate's harness against its port object (link cache is ignored)."""
    src = os.path.join(REPO, "tests", "t_port_%s.c" % gate)
    # Compile the PORT SOURCE, never the cached .o: auditing a stale object was
    # how a fresh fix kept "failing" the held-out test with the old answer.
    obj = os.path.join(REPO, "src", module + ".c")
    json_obj = os.path.join(REPO, "lib", "libjson", "json.o")
    out = os.path.join(REPO, ".testbin", "fuzz_%s.exe" % gate)
    # bash under MSYS needs forward slashes; a raw Windows path loses its
    # separators and cd fails with "No such file or directory".
    posix = lambda p: p.replace("\\", "/")
    cmd = ("export PATH=/mingw64/bin:$PATH; cd %s && gcc -std=c11 -O1 -Iinclude "
           "-Ilib/libjson -o %s %s %s %s"
           % (posix(REPO), posix(out), posix(src), posix(obj), posix(json_obj)))
    r = subprocess.run([BASH, "-lc", cmd], capture_output=True, text=True, errors="replace")
    if r.returncode != 0:
        return None, (r.stderr or r.stdout)[-400:]
    return out, None


def perturb_value(v, rng, pool):
    """Return a list of variants for one value."""
    out = []
    if isinstance(v, bool):
        out += [not v]
    if isinstance(v, int) and not isinstance(v, bool):
        out += [v + 1, v - 1, 0, -1, abs(v) + 7]
    elif isinstance(v, float):
        out += [v + 0.5, v - 0.5, 0.0, -1.0]
    elif isinstance(v, str):
        out += ["", v + "x", v[:-1], v.upper(), " " + v, v.replace("-", "_"), v * 2]
    elif v is None:
        out += ["", 0, [], {}]
    elif isinstance(v, list):
        # same-element-type variants only: a list of dicts perturbed into a list
        # of strings tests the seam, not the port, and the live side just raises.
        if v:
            out += [v[:-1], list(reversed(v)), v + [v[-1]], v + v]
            elem = v[-1]
            if isinstance(elem, dict) and elem:
                k = list(elem)[0]
                out += [v[:-1] + [dict(elem, **{k: None})]]
            elif isinstance(elem, str):
                out += [v[:-1] + [elem.upper()]]
    elif isinstance(v, dict):
        keys = list(v)
        if keys:
            out += [{k: v[k] for k in keys[:-1]}]
            out += [dict(v, **{"__extra__": 1})]
            k0 = keys[0]
            subs = [o for o in perturb_value(v[k0], rng, []) if type(o) is type(v[k0])]
            if subs:
                out += [dict(v, **{k0: subs[0]})]
    if pool:
        # transplant a value of the same JSON type seen elsewhere in this gate
        same = [x for x in pool if type(x) is type(v)]
        if same:
            out += [rng.choice(same)]
    return out


def variants_for_case(case, rng, pools, per_case):
    """Yield input-only variants of one fixture case."""
    keys = [k for k in case if k not in KEEP and not is_discriminator(k)]
    made = 0
    tries = 0
    while made < per_case and tries < per_case * 6:
        tries += 1
        v = copy.deepcopy(case)
        if not keys:
            break
        key = rng.choice(keys)
        opts = perturb_value(case[key], rng, pools.get(key, []))
        if not opts:
            continue
        v[key] = rng.choice(opts)
        v.pop("out", None)
        v["case"] = "fuzz-%s-%d" % (rng.randrange(1 << 20), made)
        v["seams"] = case.get("seams", {})
        made += 1
        yield v


def fuzz_gate(gate, module, per_case, jobs, min_cases, seed):
    fixture = os.path.join(REPO, "tests", "fixtures", "%s.jsonl" % gate)
    if not os.path.isfile(fixture):
        return gate, "no fixture", 0, 0, []
    cases = []
    with open(fixture, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            if isinstance(obj, dict) and obj.get("func"):
                cases.append(obj)
    if not cases:
        return gate, "no cases", 0, 0, []

    pools = {}
    for c in cases:
        for k, v in c.items():
            if k in KEEP:
                continue
            pools.setdefault(k, [])
            if len(pools[k]) < 12:
                pools[k].append(v)

    rng = random.Random("%s/%d" % (gate, seed))
    variants = []
    for c in cases:
        for v in variants_for_case(c, rng, pools, per_case):
            variants.append(v)
    if not variants:
        return gate, "no variants", 0, 0, []

    # Always rebuild: reusing .testbin/harnesses/<gate>.exe or o_<gate>.exe meant
    # auditing a binary built before the fix under test.
    exe, err = build_harness(gate, module)
    if not exe:
        return gate, "harness build failed: %s" % err, 0, 0, []

    fuzz_dir = os.path.join(REPO, ".testbin", "fuzz")
    os.makedirs(fuzz_dir, exist_ok=True)
    inp = os.path.join(fuzz_dir, "%s.in.jsonl" % gate)
    filled = os.path.join(fuzz_dir, "%s.filled.jsonl" % gate)
    with open(inp, "w", encoding="utf-8", newline="\n") as fh:
        for v in variants:
            fh.write(json.dumps(v, ensure_ascii=False) + "\n")

    with open(inp, "rb") as fin, open(filled, "wb") as fout:
        try:
            # A perturbed input can drive the port into an unbounded loop; without
            # a timeout the whole audit hangs instead of reporting that gate.
            r = subprocess.run([exe], stdin=fin, stdout=fout,
                               stderr=subprocess.DEVNULL, timeout=args_timeout())
        except subprocess.TimeoutExpired:
            return gate, "harness TIMEOUT (unbounded loop on a perturbed input?)", 0, 0, []
    if r.returncode not in (0, 1):
        return gate, "harness rc=%d" % r.returncode, 0, 0, []

    oracle = os.path.join(REPO, "tests", "oracle_%s.py" % gate)
    with open(filled, "rb") as fin:
        try:
            o = subprocess.run([PY, oracle], stdin=fin, capture_output=True,
                               text=True, errors="replace", timeout=args_timeout() * 2)
        except subprocess.TimeoutExpired:
            return gate, "oracle TIMEOUT", 0, 0, []
    detail = [ln for ln in (o.stderr or "").splitlines() if ln.startswith("MISMATCH")]
    real = [ln for ln in detail if "live-error" not in ln]
    noise = [ln for ln in detail if "live-error" in ln]
    total = 0
    for ln in (o.stdout or "").splitlines():
        if ln.startswith("ORACLE") and "cases," in ln:
            try:
                total = int(ln.split(":")[1].split("cases")[0].strip())
            except (IndexError, ValueError):
                pass
    note = "ok"
    if total < min_cases:
        note = "VACUOUS (%d cases < %d)" % (total, min_cases)
    return gate, note, total, len(real), real + (["(noise) " + ln for ln in noise[:2]])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gates", nargs="*")
    ap.add_argument("--plan")
    ap.add_argument("--per-case", type=int, default=6)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--min-cases", type=int, default=20)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--module", help="only for a single gate whose plan entry is unknown")
    args = ap.parse_args()

    mapping = {}
    if args.plan:
        with open(args.plan, encoding="utf-8") as fh:
            mapping = json.load(fh)
    gates = list(args.gates) or list(mapping)
    tasks = []
    for g in gates:
        m = mapping.get(g) or args.module
        if not m:
            print("skip %s: no module mapping (pass --plan or --module)" % g)
            continue
        tasks.append((g, m))

    bad = 0
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futs = [ex.submit(fuzz_gate, g, m, args.per_case, args.jobs, args.min_cases, args.seed)
                for g, m in tasks]
        for f in futs:
            gate, note, total, mism, detail = f.result()
            flag = "OK " if (note == "ok" and mism == 0) else "BAD"
            print("%s %-32s cases=%-5d value-mismatches=%-3d %s"
                  % (flag, gate, total, mism, note), flush=True)
            for ln in detail[:5]:
                print("      " + ln[:200])
            if note != "ok" or mism:
                bad += 1
    print("----\nheld-out fuzz: %d gates, %d bad" % (len(tasks), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
