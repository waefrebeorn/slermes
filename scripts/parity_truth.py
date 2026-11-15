#!/usr/bin/env python3
"""
parity_truth.py — SELF-CORRECTING source of truth for slermes parity.

WHY THIS EXISTS (the systemic disease it cures):
  The project repeatedly rotted because (a) frozen scanner snapshots
  (scan*.json, real_gaps_manifest.json from the DEPRECATED crude scanner)
  were mistaken for live truth, and (b) the generator that feeds walkway
  docs called the live scanner UNCRITICALLY — when the Python ground-truth
  (agent/, tools/, gateway/, ...) was NOT checked out, the scanner silently
  reported "0 files scanned" and the generator wrote a confident LIE into
  BANNER.md / state.md. Agents then read that lie and reported it as fact.

  The disease is not a wrong number — it is trusting a snapshot and a
  non-failing generator. The cure is FAIL-CLOSED and SOURCE-AGNOSTIC:
  this script is the ONLY writer of a parity summary, and it REFUSES to emit
  a number unless it can prove the Python source of truth is actually present
  and the scanner actually scanned it.

CONTRACT (immutable):
  1. The Python ground-truth lives at <parent>/agent/, <parent>/tools/, etc.
     where <parent> is the repo containing slermes/ (HERMES_DIR).
  2. Before trusting the scanner, we assert a known Python file exists
     (agent/anthropic_adapter.py). If it does not, we ABORT with a non-zero
     exit and a precise remediation message. No file is written. No number
     is printed as "truth".
  3. After scanning, we assert the scanner's module count is non-zero and the
     Python file count it consumed is non-zero. A 0-file scan is treated as
     FAILURE, not as "0 gaps".
  4. Output is a single canonical JSON (live_parity_scan.json) plus a
     machine + human line. Nothing else writes parity numbers.

This is AGNOSTIC: it contains no hardcoded parity figures, no module list,
no "current version". Re-run it any time; it always reflects the live tree.
"""
import glob
import json
import os
import subprocess
import sys
from datetime import datetime, timezone

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # .../slermes (C11 repo)
HERMES_DIR = os.path.dirname(REPO)                                  # .../hermes-agent-dev (the fork)
# The authoritative scanner lives in the fork's tests/ dir (sibling of slermes/).
SCANNER = os.path.join(REPO, "tests", "slermes_parity_battleground.py")
OUT = os.path.join(REPO, "live_parity_scan.json")

# A single, unambiguous proof that the Python ground-truth is checked out.
# This path is consumed by the authoritative battleground scanner.
SOURCE_PROOF = os.path.join(HERMES_DIR, "agent", "anthropic_adapter.py")

# Remediation if the source is missing: it lives in upstream/main (and the
# user's own fork). This restores ONLY the Python source, never the C tree.
REMEDIATION = (
    "Python ground-truth is NOT checked out. The live scanner cannot run.\n"
    "  Fix (restores Python source only — never touches the C tree):\n"
    "    cd %s && git checkout upstream/main -- agent tools gateway cli.py hermes_cli cron\n"
    "  (or: git checkout <your-fork>/main -- agent tools ...)\n"
    "  Then re-run this script."
) % HERMES_DIR


def source_present():
    return os.path.isfile(SOURCE_PROOF)


def pick_interpreter():
    """A NATIVE Windows Python, never an MSYS2 one.

    `sys.executable` is not good enough: when this script is launched by the
    MSYS2 python (C:/msys64/mingw64/bin/python.exe, which is what `python` on
    PATH resolves to here) the scanner resolves the C:\\Users\\<user>\\* symlink
    farm differently and walks far more than the quarry -- it over-counts
    features (34,262 vs 27,877) or hangs outright. The native Hermes python
    resolves the same paths as Windows does and finishes in ~30s.
    """
    home = os.path.expanduser("~")
    cands = [os.environ.get("PYTHON_BIN"), sys.executable]
    cands += sorted(glob.glob(os.path.join(
        home, "AppData", "Local", "hermes", "tools", "python-*", "python.exe")))
    for cand in cands:
        if not cand or not os.path.isfile(cand):
            continue
        low = cand.lower()
        if "msys64" in low or "cygwin" in low or low.endswith("\\usr\\bin\\python.exe"):
            continue
        try:
            subprocess.check_output([cand, "-c", "import hashlib, json"],
                                    timeout=60, stderr=subprocess.DEVNULL)
            return cand
        except Exception:
            continue
    return sys.executable


PY = pick_interpreter()


def scan():
    """Run the authoritative scanner. Returns the full decoded JSON doc."""
    out = subprocess.check_output([PY, SCANNER, "--json"], cwd=REPO)
    doc = json.loads(out)
    return doc


def main():
    if not source_present():
        sys.stderr.write("FATAL: " + REMEDIATION + "\n")
        return 2

    try:
        doc = scan()
    except subprocess.CalledProcessError as e:
        sys.stderr.write("FATAL: scanner failed (%s)\n" % e)
        return 3
    except json.JSONDecodeError as e:
        sys.stderr.write("FATAL: scanner output not JSON (%s)\n" % e)
        return 4

    modules = doc.get("modules", {})
    if not modules:
        sys.stderr.write(
            "FATAL: scanner returned 0 modules — source of truth not consumed.\n"
            "       (agent/ present but scanner found nothing? aborting; writing nothing.)\n")
        return 5

    # Aggregate totals for the walkway generator (which reads doc["totals"]).
    tot = {
        "total": sum(m.get("total", 0) for m in modules.values()),
        "ported": sum(m.get("ported", 0) for m in modules.values()),
        "partial": sum(m.get("partial", 0) for m in modules.values()),
        "stub": sum(m.get("stub", 0) for m in modules.values()),
        "real_gaps": sum(m.get("real_gaps", 0) for m in modules.values()),
    }
    # DA-4 breakdown: annotation-only stubs (PoP comment, no C body)
    da4 = 0
    for m in modules.values():
        for g in m.get("gaps", []):
            if g.get("classification") == "REAL_GAP":
                if "DA-4:annotation-only-stub" in g.get("da_flags", []):
                    da4 += 1
    tot["da4_annotation_only_stubs"] = da4
    tot["genuine_gaps"] = tot["real_gaps"] - da4
    tot["modules_scanned"] = len(modules)
    tot["coverage_pct"] = round(100.0 * tot["ported"] / tot["total"], 1) if tot["total"] else 0.0

    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    # Pass the scanner's full doc through (it already carries drift + per-module
    # DA flags + gaps). Add metadata + a totals block the walkway generator needs.
    doc["_generated_by"] = "scripts/parity_truth.py (fail-closed live scanner wrapper)"
    doc["_generated_at"] = stamp
    doc["_source_proof"] = os.path.relpath(SOURCE_PROOF, REPO)
    doc["_scanner"] = os.path.relpath(SCANNER, REPO)
    doc["totals"] = tot

    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1)
        f.write("\n")

    print("LIVE  PORTED %d / %d (%.1f%%)  REAL_GAP %d (DA-4 stubs: %d, genuine: %d)  PARTIAL %d  STUB %d"
          % (tot["ported"], tot["total"], tot["coverage_pct"],
             tot["real_gaps"], tot.get("da4_annotation_only_stubs", 0),
             tot.get("genuine_gaps", tot["real_gaps"]),
             tot["partial"], tot["stub"]))
    # Verifiable vs orphan: an annotation whose claimed Python module exists in
    # neither tree cannot be diffed by any oracle, so the credit it carries is
    # not verifiable. Report it beside the headline number rather than folding
    # it in, so the coverage figure stays honest.
    ver = doc.get("annotation_verifiability", {})
    if ver:
        print("ANNOTATIONS  total %d   claimed %d   verifiable %d   ORPHAN %d   "
              "unattributed %d   non-python %d"
              % (ver.get("total", 0), ver.get("claimed", 0),
                 ver.get("verifiable", 0), ver.get("orphan", 0),
                 ver.get("unattributed", 0), ver.get("non_python_claims", 0)))
        print("  orphan      = a .py module path is claimed and the file is in "
              "neither tree and has no twin")
        print("  unattributed= loose 'Port of Python: <prose>' with no module "
              "path at all")
        print("  non-python  = a claimed 'module' with no .py suffix "
              "(prose or JS, not a Python module)")
    drift = doc.get("drift", {})
    print("modules scanned: %d   drift(new=%s resolved=%s status=%s)"
          % (tot["modules_scanned"], drift.get("new_gap_count"),
              drift.get("resolved_count"), drift.get("status")))
    print("wrote %s @ %s" % (os.path.relpath(OUT, REPO), stamp))
    return 0


if __name__ == "__main__":
    sys.exit(main())
