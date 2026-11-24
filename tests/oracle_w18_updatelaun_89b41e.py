#!/usr/bin/env python3
"""Oracle for gate w18_updatelaun_89b41e (hermes_cli/update_launch_files.py)."""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

CURRENT = {"foreign": None, "systemd": False}

gateway = types.ModuleType("hermes_cli.gateway")
def _unit_path(system=False):
    return SimpleNamespace(name="system-unit" if system else "user-unit")
gateway.get_systemd_unit_path = _unit_path
gateway.get_launchd_plist_path = lambda: SimpleNamespace(name="plist")
gateway.supports_systemd_services = lambda: CURRENT["systemd"]
sys.modules["hermes_cli.gateway"] = gateway

scope = types.ModuleType("hermes_cli.update_fleet_scope")
scope.definition_foreign_checkout = lambda path: CURRENT["foreign"]
scope.announce_skipped_runtime = lambda *a, **k: None
sys.modules["hermes_cli.update_fleet_scope"] = scope

SRC = QUARRY + "/hermes_cli/update_launch_files.py"
spec = importlib.util.spec_from_file_location("w18_updatelaun_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w18_updatelaun_live"] = live
spec.loader.exec_module(live)


def main():
    cases = mism = 0
    old_platform = sys.platform
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        func = r.get("func")
        try:
            if func == "owned":
                CURRENT["foreign"] = r.get("foreign")
                py = live._owned_by_this_checkout(r.get("kind", ""))
            elif func == "refreshers":
                CURRENT["systemd"] = bool(r.get("systemd", False))
                sys.platform = r.get("platform", "")
                py = [kind for kind, _fn in live._refreshers()]
                sys.platform = old_platform
            else:
                continue
        except Exception as exc:
            sys.platform = old_platform
            print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
            continue
        cases += 1
        got = r.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
    print(f"ORACLE w18_updatelaun_89b41e: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
