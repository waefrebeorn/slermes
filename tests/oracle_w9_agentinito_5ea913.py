#!/usr/bin/env python3
"""Differential oracle for agent/agent_init_ollama.py (gate w9_agentinito_5ea913).

Imports the LIVE module via importlib with stubbed local-server/detection seams
and a counting logger; compares harness "out" to the live results.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m

agent_pkg = _pkg("agent")

class Rec:
    def __init__(self):
        self.debug_calls = 0
        self.info_calls = 0
        rec = self
        class L:
            def debug(self, *a, **k): rec.debug_calls += 1
            def info(self, *a, **k): rec.info_calls += 1
        self.logger = L()

REC = Rec()
helpers = types.ModuleType("agent.agent_runtime_helpers")
helpers._ra = lambda: REC
agent_pkg.agent_runtime_helpers = helpers
sys.modules["agent.agent_runtime_helpers"] = helpers

meta = types.ModuleType("agent.model_metadata")
STATE = {}
def _query(model, base_url, api_key=""):
    if STATE.get("detect_error"):
        raise RuntimeError("detection failed")
    return STATE.get("detected")
meta.query_ollama_num_ctx = _query
agent_pkg.model_metadata = meta
sys.modules["agent.model_metadata"] = meta

transport = types.ModuleType("agent.process_transport")
transport.is_local_server = lambda base_url, provider: STATE.get("is_local", False)
agent_pkg.process_transport = transport
sys.modules["agent.process_transport"] = transport

SRC = QUARRY + "/agent/agent_init_ollama.py"
spec = importlib.util.spec_from_file_location("w9_agentinito_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w9_agentinito_live"] = live
spec.loader.exec_module(live)


def compute(r):
    global REC
    func = r.get("func")
    REC = Rec()
    helpers._ra = lambda: REC
    STATE.clear()
    STATE["is_local"] = bool(r.get("is_local", False))
    STATE["detect_error"] = bool(r.get("detect_error", False))
    STATE["detected"] = r.get("detected")
    if func == "configure_num_ctx":
        agent = SimpleNamespace(
            model="model-x", base_url="http://localhost:11434", provider="ollama",
            api_key=r.get("api_key"), quiet_mode=bool(r.get("quiet_mode", False)))
        live._configure_ollama_num_ctx(agent, r.get("model_cfg"), r.get("config_context_length"))
        return {
            "num_ctx": getattr(agent, "_ollama_num_ctx", None),
            "info": REC.info_calls,
            "debug": REC.debug_calls,
        }
    if func == "clamp_compressor":
        calls = []
        cc = SimpleNamespace(
            context_length=r.get("cc_window"),
            update_model=lambda **kw: calls.append(kw))
        agent = SimpleNamespace(
            context_compressor=cc,
            _ollama_num_ctx=r.get("num_ctx"),
            model="model-x", base_url="http://localhost:11434", provider="ollama",
            api_key=r.get("api_key"), api_mode="chat_completions")
        live._clamp_compressor_to_ollama_num_ctx(agent)
        called = bool(calls)
        return {
            "updated": called,
            "context_length": calls[0]["context_length"] if called else None,
        }
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')}: python raised {exc!r}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
if cases == 0:
    print("ORACLE w9_agentinito_5ea913: 0 cases (link failure in disguise)")
    sys.exit(1)
print(f"ORACLE w9_agentinito_5ea913: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
