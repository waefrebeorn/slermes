#!/usr/bin/env python3
"""Differential oracle for gateway/session_vault_code_card.py (w20_sessionvau_bfb027).

The live module is imported straight from the quarry (its only imports are
stdlib), then each harness line is replayed through the real _CodeRequest,
_code_card_adapter and the author-id expression code_card_scope runs, with fake
turn/context/source objects standing in for the live ones. `out` is compared.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_sessionvau_bfb027_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "gateway", "session_vault_code_card.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class WithMethod:
    def send_vault_code_prompt(self, *a, **kw):  # pragma: no cover - never called
        return None


class MethodNone:
    send_vault_code_prompt = None


class WithoutMethod:
    pass


class Ctx:
    def __init__(self, adapter, source):
        self._status_adapter = adapter
        self.source = source


class Turn:
    def __init__(self, adapter, source):
        self._ctx = Ctx(adapter, source)


ADAPTERS = {"with": WithMethod, "none": MethodNone, "without": WithoutMethod}


def _source(spec):
    if spec is None:
        return None
    if isinstance(spec, dict):
        return types.SimpleNamespace(**spec)
    return types.SimpleNamespace()


def expected(r):
    f = r.get("func")
    if f == "code_request":
        req = live._CodeRequest()
        out = []
        for op in r.get("ops") or []:
            if not isinstance(op, dict):
                out.append({"kind": "skip"})
                continue
            kind = op.get("op")
            if kind == "submit":
                out.append({"kind": "submit", "accepted": bool(req.submit(op.get("code")))})
            elif kind == "close":
                req.close()
                out.append({"kind": "void"})
            elif kind == "cancel":
                req.cancel()
                out.append({"kind": "void"})
            elif kind == "wait":
                cancelled = bool(op.get("cancelled"))
                code = req.wait(float(op.get("timeout") or 0.0), lambda: cancelled,
                                lambda: None)
                out.append({"kind": "wait", "code": code,
                            "settled": bool(req._event.is_set())})
            elif kind == "state":
                out.append({"kind": "state", "settled": bool(req._event.is_set()),
                            "value": req._value})
            else:
                out.append({"kind": "skip"})
        return out
    if f == "user_id":
        source = _source(r.get("source"))
        ctx = types.SimpleNamespace(source=source)
        return str(getattr(getattr(ctx, "source", None), "user_id", "") or "")
    if f == "scope_gate":
        turn_spec = r.get("turn") if isinstance(r.get("turn"), dict) else {}
        adapter = (ADAPTERS.get(turn_spec.get("adapter_kind", "without"))()
                   if turn_spec.get("has_adapter") else None)
        source = _source(turn_spec.get("source"))
        turn = Turn(adapter, source)
        found = live._code_card_adapter(turn)
        user_id = str(getattr(getattr(turn._ctx, "source", None), "user_id", "") or "")
        return {"adapter": found is not None, "user_id": user_id,
                "gate": "passthrough" if (found is None or not user_id) else "install"}
    if f == "wait_slice":
        remaining = float(r.get("remaining") or 0.0)
        return min(1.0, max(0.0, remaining))
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_sessionvau_bfb027: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
