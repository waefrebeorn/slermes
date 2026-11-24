#!/usr/bin/env python3
"""Differential oracle for agent/anthropic_message_convert.py — the whole
outbound message pipeline (gate w31_anthropicm_b31360_p1).

The live module is loaded from its file in the quarry checkout; its imports
resolve against the quarry root (``agent.*``, ``tools.*``, ``utils.*``), and
``hermes_seam_stubs`` is installed first so host-blocked compiled extensions
(pydantic) cannot stop the module from loading.  Nothing is stubbed inside the
converter itself: the compared value is exactly what
``convert_messages_to_anthropic`` returns.

Seams (not compared): ``_evict_old_screenshots`` runs against
``agent/image_eviction_policy.py`` and only mutates a request that would cross
its block/byte budget; fixtures stay below that trigger.  ``rasterize_svg_data_url``
needs a soft rasterizer that is not installed on the oracle host, so an SVG data
URL always takes the placeholder branch on both sides.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_anthropicm_b31360_p1"

for _p in (str(Path(__file__).resolve().parent), str(QUARRY)):
    if _p not in sys.path:
        sys.path.insert(0, _p)

# ``pm`` is the host's packaged-runtime bootstrap: importing it runs a live
# dependency-install probe and prints to stderr. Nothing under test needs its
# install hints, and the converter's lazy ``agent.bedrock_adapter`` import pulls
# it in, so it is stubbed before anything loads.
if "pm" not in sys.modules:
    import types as _types

    _pm = _types.ModuleType("pm")
    _pm.install_hint = lambda *a, **k: ""
    sys.modules["pm"] = _pm

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location(
    "amc_p1_live", QUARRY / "agent" / "anthropic_message_convert.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["amc_p1_live"] = live
_spec.loader.exec_module(live)

WANTED = {"convert"}


def expected(r):
    messages = r.get("messages")
    system, out = live.convert_messages_to_anthropic(messages, r.get("base_url"), r.get("model"))
    return {"system": system, "messages": out}


def main():
    cases = mism = 0
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
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py =%s\n  c  =%s"
                  % (r.get("func"), r.get("case"), json.dumps(py, sort_keys=True),
                     json.dumps(got, sort_keys=True)), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
