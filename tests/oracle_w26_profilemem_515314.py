#!/usr/bin/env python3
"""Differential oracle for hermes_cli/profile_memory_config.py (gate w26_profilemem_515314).

Two live surfaces are compared:

  * ``active_memory_provider`` -- called directly on the case's parsed config;
  * ``_names_external_provider`` -- its two config sources are the seam: the
    profile's ``config.yaml`` reader (``_load_config_dict``, a module global) and
    ``managed_scope.load_managed_config`` are patched to the case's ``configs``,
    so the upstream loop (cfg_get + is_core_memory_provider) runs verbatim
    without touching a file.

The copy (copytree / copy2 / chmod) never runs: it is the file-system seam.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_profilemem_515314"

os.environ["USERPROFILE"] = str(REPO.parent)
os.environ["HOME"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import managed_scope  # noqa: E402
from hermes_cli import profile_memory_config as live  # noqa: E402

WANTED = {"active_provider", "names_external", "constants"}

CASE = {"configs": []}


def patched_load_config_dict(profile_dir):
    configs = CASE["configs"]
    return configs[0] if configs else None


def patched_load_managed_config():
    configs = CASE["configs"]
    return configs[1] if len(configs) > 1 else None


live._load_config_dict = patched_load_config_dict
managed_scope.load_managed_config = patched_load_managed_config


def expected(r):
    f = r.get("func")
    if f == "active_provider":
        return live.active_memory_provider(r.get("config"))
    if f == "names_external":
        raw = r.get("configs")
        CASE["configs"] = raw if isinstance(raw, list) else []
        return live._names_external_provider(REPO)
    if f == "constants":
        from agent.memory_provider import CORE_MEMORY_PROVIDER_SENTINELS
        return {
            "core_memory_provider_sentinels": sorted(CORE_MEMORY_PROVIDER_SENTINELS),
            "provider_name_pattern": live._PROVIDER_NAME_RE.pattern,
        }
    raise AssertionError("unknown func %r" % (f,))


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
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
