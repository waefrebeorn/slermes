#!/usr/bin/env python3
"""Differential oracle for gate w16_runinbound_e075b2 (gateway/run_inbound_consumer.py)."""
import asyncio, importlib.util, json, logging, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
logging.disable(logging.CRITICAL)
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
def _mod(name):
    m = types.ModuleType(name); m.__path__ = [os.path.join(Q, name)]; sys.modules[name] = m; return m
_mod("gateway")
cli = _mod("hermes_cli")
life = types.ModuleType("hermes_cli.lifecycle")

def _make_lifecycle(record):
    def has_hook(name): return record.get("mode") != "has_hook_false"
    async def ainvoke_hook(name, **kw):
        if record.get("mode") == "hook_error": raise RuntimeError("boom")
        return record.get("results") or []
    life.has_hook = has_hook
    life.ainvoke_hook = ainvoke_hook
sys.modules["hermes_cli.lifecycle"] = life
cli.lifecycle = life

NAME = "w16_runinbound_e075b2_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "gateway", "run_inbound_consumer.py"))
live = importlib.util.module_from_spec(sp); sys.modules[NAME] = live; sp.loader.exec_module(live)
NS = types.SimpleNamespace

def canon(v):
    if isinstance(v, (list, tuple)): return [canon(x) for x in v]
    if isinstance(v, dict): return {k: canon(x) for k, x in v.items()}
    return v

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "hook_choice":
            _make_lifecycle(r)
            runner = NS(config=NS(multiplex_profiles=False))
            source = NS(platform=NS(value="telegram"), to_dict=lambda: {"chat_id": "c1"})
            event = NS(message_id="m1", text="hello")
            handled, reply = asyncio.run(
                live.run_post_admission_hook(runner, event, source, r.get("session_key") or "s1"))
            py = {"handled": handled, "reply": reply}
        else: continue
        py = canon(py)
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w16_runinbound_e075b2: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
