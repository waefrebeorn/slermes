#!/usr/bin/env python3
"""Oracle for gate w18_webserver_e8a789 (hermes_cli/web_server_boundary.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

# Parent-package bootstrap: register a stub hermes_cli package before loading
# the live module by file location (the module uses only os/re/urllib.parse).
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

SRC = QUARRY + "/hermes_cli/web_server_boundary.py"
spec = importlib.util.spec_from_file_location("w18_wsb_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w18_wsb_live"] = live
spec.loader.exec_module(live)


def frozenset_of(value):
    return frozenset(value or ())


def main():
    cases = mism = 0
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
            if func == "public_url_hosts":
                py = sorted(live.public_url_hosts(r.get("url", "")))
            elif func == "host_header":
                py = live._host_header_hostname(r.get("header", ""))
            elif func == "is_accepted":
                py = live._is_accepted_host(
                    r.get("header", ""), r.get("bound", ""),
                    frozenset_of(r.get("trusted")))
            elif func == "should_auth":
                py = live.should_require_auth(r.get("host", ""),
                                              bool(r.get("allow_public", False)))
            elif func == "engage_gate":
                py = live.public_hosts_engage_gate(frozenset_of(r.get("trusted")))
            elif func == "loopback_origin":
                o = live._loopback_origin(r.get("url", ""))
                py = None if o is None else [o[0], o[1], o[2]]
            elif func == "public_url_origins":
                py = sorted([list(o) for o in live.public_url_origins(r.get("url", ""))])
            else:
                continue
        except Exception as exc:
            print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
            continue
        cases += 1
        got = r.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
    print(f"ORACLE w18_webserver_e8a789: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
