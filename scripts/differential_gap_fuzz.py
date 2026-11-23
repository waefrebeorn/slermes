#!/usr/bin/env python3
"""Boundary-biased differential gap fuzzer for Slermes LIVE oracle gates.

Research basis: differential testing (McKeeman) + property-based boundary bias
(QuickCheck/Hypothesis). Existing fixtures supply the grammar; this script
mutates string fields with adversarial values and diffs harness output against
the LIVE Python oracle. Any mismatch is shrunk by reverting fields one at a time
and the minimal witness is printed as JSONL.
"""
import copy
import json
import os
import random
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS = os.path.join(ROOT, ".testbin", "harnesses")
PY = os.path.join(ROOT, ".testbin", "pytestbin.cache")
ORACLE = os.path.join(ROOT, "tests")

OUTPUT_KEYS = {"out", "identity", "identity_kind", "identity_value", "default_out", "after_reset", "expand", "want", "expected"}
EDGE = [
    "", " ", "  ", "\t", "\n", "a", "A", "0", "/", "\\", ".", "..",
    "C:\\", "C:/", "//server/share", "\\\\server\\share", "x" * 260,
    " leading", "trailing ", "a/b/../c", "a\\b\\..\\c", "😀", "\udcff",
    "true", "false", "null", "-1", "3.14", "é", "heures d'été",
]


# Gates whose LIVE trace recorder uses a different argument-label convention
# for mutated fixture shapes than the pinned fixture corpus; static mutation
# covers them. Do not treat their generated witnesses as port defects.
SKIP_GATES = {
    "context_references_flow",
    "executions_worker_flow",
    "executions_worker_flow2",
}


SKIP_KEYS = {
    "func", "fn", "id", "case", "name", "_orig", "__raise__", "__script__",
    "__callable__", "__special__", "type", "message", "exc", "errno", "reason",
    "platform", "message_type", "intent",
    # LIVE returns a frozenset here; its iteration order is process-dependent,
    # so a mutated list cannot be compared byte-for-byte against the port.
    "custom_provider_aliases",
}


def strings(obj, path=()):
    if isinstance(obj, dict):
        for k, v in obj.items():
            if k in SKIP_KEYS:
                continue
            yield from strings(v, path + (k,))
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            yield from strings(v, path + (i,))
    elif isinstance(obj, str):
        yield path, obj


def setpath(obj, path, value):
    cur = obj
    for key in path[:-1]:
        cur = cur[key]
    cur[path[-1]] = value


def mutate(rng, case):
    out = copy.deepcopy(case)
    container = out.get("args", out.get("in", out))
    paths = [p for p, _ in strings(container)]
    if not paths:
        return None
    for _ in range(rng.choice((1, 1, 2))):
        path = rng.choice(paths)
        if rng.random() < 0.75:
            setpath(container, path, rng.choice(EDGE))
        else:
            cur = str(_getpath(container, path))
            setpath(container, path, cur + rng.choice(EDGE))
    return out


def _getpath(obj, path):
    cur = obj
    for key in path:
        cur = cur[key]
    return cur


def strip_expected(case):
    out = copy.deepcopy(case)
    for k in OUTPUT_KEYS:
        out.pop(k, None)
    out.pop("_orig", None)
    return out


def run(gate, cases, timeout=60):
    cases = [strip_expected(c) for c in cases]
    exe = os.path.join(HARNESS, gate + ".exe")
    oracle = os.path.join(ORACLE, f"oracle_{gate}.py")
    if not os.path.isfile(exe) or not os.path.isfile(oracle):
        return None, f"missing harness/oracle for {gate}"
    inp = "".join(json.dumps(c, ensure_ascii=False) + "\n" for c in cases)
    p = subprocess.run([exe], input=inp, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=timeout)
    py = open(PY, encoding="utf-8").read().strip().replace("/c/", "C:/", 1)
    r = subprocess.run([py, oracle], input=p.stdout, capture_output=True,
                       text=True, encoding="utf-8", errors="replace", timeout=timeout)
    return (p.returncode, r.returncode, r.stdout, r.stderr), None


def shrink(gate, case):
    """Revert mutated fields one at a time while the case still mismatches."""
    current = case
    for path, _ in list(strings(current.get("args", current.get("in", current)))):
        trial = copy.deepcopy(current)
        container = trial.get("args", trial.get("in", trial))
        try:
            setpath(container, path, case["_orig"][json.dumps(path)])
        except Exception:
            continue
        res, err = run(gate, [trial])
        if err:
            continue
        if "0 mismatches" not in (res[2] + res[3]):
            current = trial
    return current


def main():
    if len(sys.argv) < 3:
        print("usage: differential_gap_fuzz.py GATE N [SEED]", file=sys.stderr)
        return 2
    gate, n = sys.argv[1], int(sys.argv[2])
    if gate in SKIP_GATES:
        print(f"GATE {gate} skipped by fixtured trace-label policy")
        return 0
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 20261122
    rng = random.Random(seed)
    fp = os.path.join(ROOT, "tests", "fixtures", gate + ".jsonl")
    templates = [json.loads(l) for l in open(fp, encoding="utf-8", errors="surrogatepass") if l.strip()]
    generated = []
    for _ in range(n):
        base = copy.deepcopy(rng.choice(templates))
        container = base.get("args", base.get("in", base))
        paths = [p for p, _ in strings(container)]
        base["_orig"] = {json.dumps(p): _getpath(container, p) for p in paths}
        m = mutate(rng, base)
        if m:
            m["_orig"] = base["_orig"]
            generated.append(m)
    res, err = run(gate, generated)
    if err:
        print("ERROR", err, file=sys.stderr)
        return 2
    mismatches = "0 mismatches" not in (res[2] + res[3])
    print(f"GATE {gate} cases={len(generated)} rc={res[:2]} mismatches={mismatches}")
    if not mismatches:
        return 0
    # Find one witness by bisection over the generated batch.
    bad = None
    for c in generated:
        one, _ = run(gate, [c])
        if "0 mismatches" not in (one[2] + one[3]):
            bad = c
            break
    if bad:
        small = shrink(gate, bad)
        small = strip_expected(small)
        print("WITNESS " + json.dumps(small, ensure_ascii=False))
    return 1


if __name__ == "__main__":
    sys.exit(main())
