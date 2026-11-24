#!/usr/bin/env python3
"""Differential oracle for hermes_platform/host/products.py
(gate w24_products_7eaca0).

The live module is imported whole; the host-fact seam (``facts.nvidia_gpus`` et
al.) is fed from the fixture as real ``facts.NvidiaGpu`` named tuples, so the
recognition ladder under test is upstream's.  ``facts.*`` itself is never
called: no registry, sysfs, procfs or sysctl read happens.

A live TypeError/AttributeError is the raise seam -- the C port has no exception
path there, so the expected value is null.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_products_7eaca0"

# git-bash login shells carry no USERPROFILE, which Path.home() needs.
if not (os.environ.get("USERPROFILE") or os.environ.get("HOMEDRIVE")):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_platform.host import products as live                # noqa: E402
from hermes_platform.host import facts                          # noqa: E402

WANTED = {"is_nvidia_n1x_pci_id", "classify_nvidia_platform", "constants"}

SEAM_ERRORS = (TypeError, ValueError, AttributeError)


def is_nvidia_n1x_pci_id(r):
    return live.is_nvidia_n1x_pci_id(r.get("pci_id"))


def classify_nvidia_platform(r):
    gpus = tuple(facts.NvidiaGpu(name=g.get("name"), pci_id=g.get("pci_id"))
                 for g in (r.get("nvidia_gpus") or []))
    return live.classify_nvidia_platform(
        os_family=r.get("os_family", ""),
        native_arch=r.get("native_arch", ""),
        cpu_model=r.get("cpu_model", ""),
        gpu_class=r.get("gpu_class", ""),
        nvidia_gpus=gpus,
    )


def constants():
    return {
        "rtx_spark_cpu_marker": live._RTX_SPARK_CPU_MARKER,
        "dgx_spark_markers": list(live._DGX_SPARK_MARKERS),
    }


def expected(r):
    f = r.get("func")
    if f == "is_nvidia_n1x_pci_id":
        return is_nvidia_n1x_pci_id(r)
    if f == "classify_nvidia_platform":
        return classify_nvidia_platform(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = seams = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except SEAM_ERRORS as exc:
            seams += 1
            if r.get("out") is not None:
                mism += 1
                print("MISMATCH %s case=%r live raises %r but c=%r (expected null)"
                      % (r.get("func"), r.get("case"), exc, r.get("out")), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
