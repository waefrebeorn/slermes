#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_chat_view.py (gate w16_gatewaycha_19399b)."""
import importlib.util
import json
import re
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_gatewaycha_19399b"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


class GatewayClientError(Exception):
    pass


# Only dependency: GatewayClientError (error identity) and a CSI strip_ansi seam.
# The strip seam covers the escape shapes present in the fixtures.
_seed("hermes_cli")
_seed("hermes_cli.gateway_client", GatewayClientError=GatewayClientError,
      COMPRESS_RPC_TIMEOUT=120.0)
_seed("tools")
_seed("tools.ansi_strip", strip_ansi=lambda s: (
    re.sub(r"\x1b\[[\x30-\x3f]*[\x20-\x2f]*[\x40-\x7e]|\x1b[\x30-\x7e]", "", s)
    if s else s))

SRC = QUARRY + "/hermes_cli/gateway_chat_view.py"
_spec = importlib.util.spec_from_file_location("gateway_chat_view_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["gateway_chat_view_live"] = live
_spec.loader.exec_module(live)


def _caught(fn):
    try:
        return fn()
    except Exception as exc:
        return {"__raise__": type(exc).__name__, "msg": str(exc)}


def live_out(rec):
    func = rec.get("func")
    if func == "preview":
        return live._pending_preview(rec.get("text"))
    if func == "quoted":
        return live._quoted(rec.get("row") or {}, rec.get("fallback", "lost turn"))
    if func == "lines":
        return live.pending_lines(rec.get("pending") or [],
                                  interactive=bool(rec.get("interactive", True)))
    if func == "lost":
        return _caught(lambda: live.lost_admission(rec.get("snapshot") or {},
                                                   rec.get("id", "")))
    if func == "oneshot":
        return _caught(lambda: live._oneshot_admission(rec.get("receipt")))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("preview", "quoted", "lines", "lost", "oneshot"):
            continue
        cases += 1
        py = live_out(rec)
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
