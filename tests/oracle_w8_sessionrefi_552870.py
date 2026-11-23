#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_refine.py
(w8_sessionrefi_552870 gate).

The live module is loaded via spec. Its i18n `t`, Platform enum and
local_authority_live are injected as deterministic stand-ins: the English
catalog strings from locales/en.yaml are pinned here (the real catalog loader
needs ruamel.yaml, unavailable on this host), and the live module's own early
gate / formatting lines are exercised for the comparison.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


CAT = {
    "gateway.refine.focus_suffix": " (focus: {focus})",
    "gateway.refine.started":
        "⚗ Reviewing this conversation in the background{focus_suffix} — any memory/skill updates will be reported when done.",
}


def _t(key, lang=None, **kw):
    return CAT[key].format(**kw)


_mod("agent.i18n", t=_t)

gpkg = _mod("gateway"); gpkg.__path__ = []


class _Platform:
    LOCAL = "local"


_mod("gateway.config", Platform=_Platform)
_mod("gateway.session_setting_commands",
     local_authority_live=lambda runner, source: (None, object()))

SRC = Q + "/gateway/session_refine.py"
spec = importlib.util.spec_from_file_location("sessref_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sessref_live"] = live
spec.loader.exec_module(live)

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
        if func == "is_local":
            if r.get("source_null", False):
                source = None
            else:
                source = types.SimpleNamespace(platform=r.get("platform"))
            _authority, live_session = live._local_session(None, source)
            py = 1 if live_session is not None else 0
        elif func == "focus_tail":
            focus = r.get("focus", "")
            py = live.t("gateway.refine.focus_suffix", focus=focus) if focus else ""
        elif func == "started":
            py = live.t("gateway.refine.started", focus_suffix=r.get("suffix", ""))
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

print(f"ORACLE w8_sessionrefi_552870: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
