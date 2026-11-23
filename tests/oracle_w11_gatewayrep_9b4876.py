#!/usr/bin/env python3
"""Differential oracle for acp_adapter/gateway_reply.py (gate w11_gatewayrep_9b4876)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
SN = types.SimpleNamespace

# --- stubs for imports touched at module load / inside the methods --------
acp = types.ModuleType("acp")
acp.update_agent_message_text = lambda *a, **k: None
acp.update_agent_thought_text = lambda *a, **k: None
acp.update_tool_call = lambda *a, **k: None
sys.modules["acp"] = acp

hc = types.ModuleType("hermes_cli")
hc.__path__ = []
sys.modules["hermes_cli"] = hc
grg = types.ModuleType("hermes_cli.gateway_replay_gap")
def _owed_reply(shown, final):
    head = shown.removeprefix("\n\n")
    for prefix in (head, head.rstrip()):
        if final.startswith(prefix):
            return final[len(prefix):]
    return "\n[output resumed]\n" + final
grg.owed_reply = _owed_reply
sys.modules["hermes_cli.gateway_replay_gap"] = grg

agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = []
sys.modules["agent"] = agent_pkg
cl = types.ModuleType("agent.conversation_loop")
cl.INTERRUPT_WAITING_FOR_MODEL_PREFIX = "Operation interrupted: waiting for model response ("
sys.modules["agent.conversation_loop"] = cl

SRC = QUARRY + "/acp_adapter/gateway_reply.py"
spec = importlib.util.spec_from_file_location("w11_gatewayrep_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_gatewayrep_live"] = live
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
    got = r.get("out")
    try:
        if func == "text":
            want = live._text(r.get("payload", {}))
        elif func == "close_segment":
            shown = r.get("shown", [])
            slf = SN(_streamed={"A": r.get("streamed", "")}, _segments={"A": list(shown)})
            live.GatewayReplyMixin._close_segment(slf, "A")
            want = slf._segments["A"]
        elif func == "final_remainder":
            cut = r.get("cut", None)
            payload = {"text": r.get("text", ""), "response_reused": r.get("response_reused", False)}
            if r.get("outcome", None) is not None:
                payload["outcome"] = r.get("outcome")
            if r.get("has_receipt", False):
                payload["receipt"] = {"status": "terminal"}
            shown = r.get("shown", [])
            slf = SN(_streamed={"A": r.get("streamed", "")},
                     _segments={"A": list(shown)},
                     _cut=({"A": cut} if cut is not None else {}))
            want = live.GatewayReplyMixin._final_remainder(slf, "A", payload)
        else:
            continue
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_gatewayrep_9b4876: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
