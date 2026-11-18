#!/usr/bin/env python3
"""Oracle for the pure slice of hermes_cli/left_core_migration.py."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from urllib.parse import urlparse

# left_core_migration._at_host imports utils.base_url_host_matches lazily, and
# importing the quarry's utils pulls in ruamel (absent here). Supply the same
# function directly so the comparison stays about the port's own logic.
# Mirrors LIVE utils.py: base_url_host_matches -> base_url_hostname ->
# _hostname_of, which LOWERCASES and RSTRIPS "." from the hostname (a stale
# copy without the rstrip reported false FAPs on trailing-dot hosts).
def _bh(base_url, domain):
    raw = (base_url or "").strip()
    parsed = urlparse(raw if "://" in raw else "//" + raw) if raw else None
    hostname = (parsed.hostname or "").lower().rstrip(".") if parsed else ""
    domain = (domain or "").strip().lower().rstrip(".")
    return bool(hostname and domain) and (hostname == domain
                                          or hostname.endswith("." + domain))


import types as _types                                    # noqa: E402
_utils_stub = _types.ModuleType("utils")
_utils_stub.base_url_host_matches = _bh
sys.modules.setdefault("utils", _utils_stub)

from hermes_cli import left_core_migration as lcm   # noqa: E402


def main() -> int:
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        try:
            if fn == "toolset_listed":
                want = lcm._toolset_listed(i["config"], frozenset(i["names"]),
                                           own_platform=i.get("own_platform") or "")
            elif fn == "delegation_backend_routes":
                want = lcm._delegation_backend_routes(i["delegation"])
            elif fn == "at_host":
                try:
                    want = lcm._at_host(i["url"], i["host"])
                except ValueError:
                    want = False
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        raw = c["out"]
        got = raw["str"] if isinstance(raw, dict) and set(raw) == {"str"} else raw
        if got != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:300]} "
                  f"PY={want!r}")
    if total == 0:
        print("ORACLE left_core_migration: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE left_core_migration: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
