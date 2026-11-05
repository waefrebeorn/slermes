#!/usr/bin/env python3
"""Oracle for agent/delegation_registry.py: _bundled_disabled + backend_status
reason strings.

Both take runtime facts (the disabled plugin set, provider availability/
registered) that cannot be loaded without the plugin manager, so the
transforms and the exact message strings are transcribed from the source as the
independent reference (the strings are copied verbatim).
"""
import json
import sys

DEFAULT_BACKEND = "default"

DISABLED_DEFAULT = (
    "No default delegation strategy is available: the bundled plugin "
    "delegation/default is disabled. Remove it from plugins.disabled in "
    "config.yaml.")
NOT_INSTALLED = (
    "Delegation backend '{name}' is not installed: no enabled plugin provides "
    "it. Install or enable the plugin that does (`hermes plugins list`), or "
    "switch back to the default backend (`hermes config set "
    "delegation.backend default`).")
UNAVAILABLE = (
    "Delegation backend '{name}' is unavailable here (its plugin reports it "
    "cannot run). Fix its setup, or switch back to the default backend: "
    "`/delegation default` for this session, or `hermes config set "
    "delegation.backend default`.")


def bundled_disabled(name, disabled_list):
    disabled = set((disabled_list or "").replace(",", " ").split())
    return bool({name, f"delegation/{name}"} & disabled)


def status_reason(name, available, registered):
    if available:
        return ""
    if not registered and name == DEFAULT_BACKEND:
        return DISABLED_DEFAULT
    if not registered:
        return NOT_INSTALLED.format(name=name)
    return UNAVAILABLE.format(name=name)


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE delegation_registry: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "bd":
            want = bundled_disabled(c["name"], c["list"])
            if (c["out"] == 1) != want:
                mism += 1
                print(f"MISMATCH {c['case']}: C={c['out']} PY={want} "
                      f"name={c['name']!r} list={c['list']!r}")
        elif fam == "bs":
            want = status_reason(c["name"], bool(c["available"]),
                                 bool(c["registered"]))
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {c['case']}: C={c['out']!r} "
                      f"PY={want!r} name={c['name']!r}")
    print(f"ORACLE delegation_registry: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
