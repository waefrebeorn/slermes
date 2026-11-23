#!/usr/bin/env python3
"""Differential oracle for gate w14_sessionbot_ecd66b (gateway/session_bot_mailbox.py)."""
import contextlib
import copy
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_sessionbot_ecd66b"
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


for _pkg in ("gateway", "persistence", "persistence.core", "tools"):
    if _pkg not in sys.modules:
        _seed(_pkg)


class AuthorityRefusal(Exception):
    def __init__(self, reason):
        super().__init__(reason)
        self.reason = reason


_seed("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
_seed("tools.bot_failure_reasons", AGENT_BLOCKED="agent_blocked")

# Stub the file-backed primitives: live decisions run unchanged against a JSON seam.
class _FakePath:
    def __truediv__(self, other):
        return self
    def unlink(self):
        pass


_seed("tools.bot_live_delivery",
      _locked=lambda home: contextlib.nullcontext(_FakePath()),
      _read=lambda path: None,
      _write=lambda path, value: None)

SRC = QUARRY + "/gateway/session_bot_mailbox.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def _wire(current, writes):
    live._locked = lambda home: contextlib.nullcontext(_FakePath())
    live._read = lambda path: copy.deepcopy(current)
    live._write = lambda path, value: writes.append(copy.deepcopy(value))


def compute(r):
    func = r.get("func")
    rec = r.get("record") or {}
    cur = r.get("current")
    if func == "write_receipt":
        writes = []
        _wire(cur, writes)
        try:
            if r.get("expect_present"):
                res = live._write_receipt("/h", rec, r.get("expect"))
            else:
                res = live._write_receipt("/h", rec)
            return {"result": res, "error": None, "wrote": writes[-1] if writes else None}
        except AuthorityRefusal as exc:
            return {"result": None, "error": exc.reason, "wrote": None}
    if func == "withdraw_pin":
        _wire(cur, [])
        return live._withdraw_pin("/h", "k")
    if func == "requeue_pin":
        _wire(cur, [])
        return live._requeue_pin("/h", "k")
    if func == "refuse_pin":
        writes = []
        _wire(cur, writes)
        live._refuse_pin("/h", "k")
        return writes[-1] if writes else None
    if func == "remember_receipt":
        auth = NS(_bot_receipt_index=copy.deepcopy(r.get("index") or {}))
        live.remember_receipt(auth, r.get("home"), rec)
        return auth._bot_receipt_index
    return None


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


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
        print(f"MISMATCH {G} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
