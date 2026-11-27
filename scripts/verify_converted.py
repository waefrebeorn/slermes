"""Apply the unprint conversion to a batch, verify each gate, keep only the greens.

Converting a print-and-return-0 function is only worth anything if the gate still
agrees with Python afterwards. This runs the batch end to end:

    apply -> rebuild -> run each gate -> revert every gate that is not MATCH

A gate that goes red after conversion is a real divergence and is reported with its
verdict; it is never left in the tree half-converted.

Usage:
    python scripts/verify_converted.py --list .testbin/safe_batch.txt [--link full]
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "scripts"))
import unprint_answers as unp  # noqa: E402

HEAD = re.compile(r"^-- (\S+) \(subsystem: [^)]*\) --")


def rebuild() -> int:
    cmd = ("cd /c/Users/eman5/slermes && export PATH=\"/c/msys64/mingw64/bin:$PATH\" && "
           "source build/winlibs-env.sh >/dev/null 2>&1; make -j8 "
           "USERPROFILE=\"C:\\Users\\eman5\" LOCALAPPDATA=\"C:\\Users\\eman5\\AppData\\Local\" "
           "2>&1 | grep -E \"error:\" | head -8")
    r = subprocess.run(["C:/msys64/usr/bin/bash.exe", "-lc", cmd], capture_output=True, text=True)
    errs = [ln for ln in r.stdout.splitlines() if "error:" in ln]
    for ln in errs[:8]:
        print("  BUILD", ln.strip())
    return len(errs)


def run_gates(gates: list[str], link: str) -> dict[str, str]:
    env = dict(os.environ)
    env["WATERCUP_LINK"] = link
    proc = subprocess.run([sys.executable, "-u", "tests/watercup/wc_run.py", *gates],
                          cwd=str(REPO), env=env, capture_output=True, text=True)
    out = proc.stdout + proc.stderr
    out = re.sub(r"(?m)^-- \S+ \(subsystem: [^)]*\) --\n\s*-> all \d+ cups MATCH LIVE Python", "--X", out)
    verdicts: dict[str, str] = {}
    for b in re.split(r"(?m)^(?=-- \S+ \(subsystem: )", out):
        m = HEAD.match(b)
        if not m:
            continue
        gate = m.group(1)
        if "NON-DISCRIMINATING" in b:
            verdicts[gate] = "NON-DISC"
        elif "TWIN BLIND" in b:
            verdicts[gate] = "TWIN-BLIND"
        elif "all " in b and "MATCH LIVE Python" in b:
            verdicts[gate] = "MATCH"
        elif "CUP MISMATCH" in b or "cup mismatches" in b:
            verdicts[gate] = "MISMATCH"
        elif "ERROR" in b:
            verdicts[gate] = "ERROR"
        else:
            verdicts[gate] = "UNKNOWN"
    return verdicts


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", required=True)
    ap.add_argument("--link", default="full")
    ap.add_argument("--skip-build", action="store_true")
    a = ap.parse_args()

    gates = [ln.strip() for ln in Path(a.list).read_text().splitlines() if ln.strip()]
    applied: list[str] = []
    for g in gates:
        v = unp.convert_gate(g, True)
        print(f"  {g[:66]:68s} {v}")
        if v == "converted":
            applied.append(g)
    if not applied:
        print("nothing converted")
        return 1

    if not a.skip_build:
        errs = rebuild()
        if errs:
            print(f"build failed ({errs} errors) -- reverting all conversions")
            for g in applied:
                unp.revert(g)
            return 2

    verdicts = run_gates(applied, a.link)
    keep, drop = [], []
    for g in applied:
        v = verdicts.get(g, "NOT-RUN")
        (keep if v in ("MATCH", "NON-DISC") else drop).append((g, v))

    print("\n== converted and verified")
    for g, v in keep:
        print(f"  {v:10s} {g}")
    print("== converted but rejected (divergence, reverted)")
    for g, v in drop:
        print(f"  {v:10s} {g}")
        unp.revert(g)

    Path(".testbin/verify_converted.json").write_text(json.dumps(
        {"kept": [g for g, _ in keep], "dropped": [{"gate": g, "verdict": v} for g, v in drop]},
        indent=1))
    print(f"\nkept {len(keep)}  reverted {len(drop)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
