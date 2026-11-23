#!/usr/bin/env python3
"""Differential oracle for tools/bot_mode_dm_chain.py (gate w10_botmodedm_ebba71)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w10_botmodedm_ebba71"
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


# Parent-package bootstrap for the quarry import chain used by recipient helpers.
for _pkg in ("tools", "hermes_cli", "agent"):
    try:
        __import__(_pkg)
    except Exception:
        _seed(_pkg)

CFG = {"doc": {}}
_seed("hermes_cli.config_backend",
      read_config_doc_readonly=lambda path: CFG["doc"])

SRC = QUARRY + "/tools/bot_mode_dm_chain.py"
_spec = importlib.util.spec_from_file_location("bot_mode_dm_chain_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["bot_mode_dm_chain_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "definitely_not_sent":
        return live._definitely_not_sent(rec.get("ack"))
    if func == "peer_key":
        author = rec.get("turn_author")
        url = rec.get("peer_url") or ""
        CFG["doc"] = {"bot_peers": {rec.get("peer_name"): {"url": url}}}
        sender = types.SimpleNamespace(
            agent=types.SimpleNamespace(_turn_author=author), root=Path("/r"))
        return live._peer_key(rec.get("peer_name"), rec.get("peer_profile"), sender)
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
        if rec.get("func") not in ("definitely_not_sent", "peer_key"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
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
