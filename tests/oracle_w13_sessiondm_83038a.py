#!/usr/bin/env python3
"""Differential oracle for gate w13_sessiondm_83038a (gateway/session_dm_threads.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_sessiondm_83038a"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = sys.modules.get(".".join(parts[:i]))
        if parent is not None and not hasattr(parent, parts[i]):
            setattr(parent, parts[i], mod)
    return mod


if "gateway" not in sys.modules:
    _seed("gateway")
_seed("gateway.config", Platform=NS(TELEGRAM="telegram", LOCAL="local", DISCORD="discord"))


def replace_source(src, **kw):
    d = dict(vars(src))
    d.update(kw)
    return NS(**d)


_seed("gateway.session_identity", replace_source=replace_source,
      identity_of=lambda s: None)

SRC = QUARRY + "/gateway/session_dm_threads.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "declared_topic":
        return live._declared_topic_thread(r.get("extra") or {}, r.get("chat_id"), r.get("thread_id"))
    if func == "key_source":
        src = NS(**(r.get("source") or {}))
        db = None
        if isinstance(r.get("db"), dict):
            db = NS(routing=NS(is_topic_mode_enabled=lambda **kw: r["db"].get("topic_mode") is True))
        out = live.dm_thread_key_source(src, r.get("extra"), db)
        return getattr(out, "thread_id", None)
    if func == "batch_suffix":
        src = NS(**(r.get("source") or {}))
        return live.dm_thread_batch_suffix(src, r.get("extra"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
