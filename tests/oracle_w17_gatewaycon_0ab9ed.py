#!/usr/bin/env python3
"""Differential oracle for gate w17_gatewaycon_0ab9ed (hermes_cli/gateway_connection.py)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "hermes_cli" / "gateway_connection.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)
_pkg = types.ModuleType("hermes_cli")
_pkg.__path__ = [str(Path(QUARRY) / "hermes_cli")]
sys.modules["hermes_cli"] = _pkg

class _GatewayClientError(Exception):
    pass

class _GatewayRPCError(_GatewayClientError):
    pass

_gc = types.ModuleType("hermes_cli.gateway_client")
_gc.DEFAULT_RPC_TIMEOUT = 30
_gc.DISCONNECTED = "disconnected"
_gc.NOT_SENT = "not_sent"
_gc.RESUME_KEY_MISMATCH = "resume_key_mismatch"
_gc.GatewayClientError = _GatewayClientError
_gc.GatewayRPCError = _GatewayRPCError
sys.modules["hermes_cli.gateway_client"] = _gc

_SPEC = importlib.util.spec_from_file_location(
    "w17_gatewaycon_live", str(Path(QUARRY) / "hermes_cli" / "gateway_connection.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, list): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def expected(rec):
    f = rec.get("func")
    if f == "lost_before":
        ce = rec.get("connection_epoch")
        ep = rec.get("epoch")
        ce = int(ce) if ce is not None else None
        ep = int(ep) if ep is not None else None
        if rec.get("is_exception"):
            frame = RuntimeError("transport")
            frame.connection_epoch = ce
        else:
            frame = {"connection_epoch": ce}
        return bool(mod.lost_before(frame, ep))
    if f == "runs_route":
        st = mod.SessionState("s")
        st.info = dict(rec.get("info") or {})
        return bool(st.runs_route(rec.get("model"), rec.get("provider") or ""))
    if f == "apply_event":
        st = mod.SessionState("s")
        st.apply_event(rec.get("kind"), rec.get("payload"))
        return {"info": st.info, "title": st.title, "pending": st.pending,
                "pending_seq": st.pending_seq, "todo_state": st.todo_state,
                "usage": st.usage, "gaps": st.gaps}
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        want = expected(rec)
        if _canon(got) != _canon(want):
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w17_gatewaycon_0ab9ed: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
