#!/usr/bin/env python3
"""Differential oracle for gateway/run_inbound_moa.py (_hm_cmd_moa).

Drives the LIVE async handler with scripted collaborators (injected via the
harness fixture) and compares handled flag, reply text and the post-call
event.text against the C decision port. Fail-closed.
"""
import asyncio, importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/gateway/run_inbound_moa.py"
spec = importlib.util.spec_from_file_location("moa_w9_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["moa_w9_live"] = live
spec.loader.exec_module(live)


def stubmod(name, **kw):
    parts = name.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules:
            m = types.ModuleType(dn); m.__path__ = []; sys.modules[dn] = m
    m = sys.modules[name]
    for k, v in kw.items():
        setattr(m, k, v)


def compute(r):
    args = r.get("command_args", "")
    preset = r.get("preset", "")
    usage = r.get("usage_text")
    managed_text = r.get("managed_text")
    failed_text = r.get("prepare_failed_text")
    policy_present = bool(r.get("policy_present"))
    active = bool(r.get("active_sink_present"))
    managed = bool(r.get("managed"))
    rebind_ok = bool(r.get("rebind_ok"))
    admit_ok = bool(r.get("admit_ok"))
    admit_raises = bool(r.get("admit_raises"))

    async def default_preset():
        return preset

    def moa_usage():
        return usage

    def policy_for_source(*a, **k):
        return object() if policy_present else None

    def active_sink():
        return object() if active else None

    def policy_if_managed(p):
        return object() if managed else None

    def rebind_model_override(*a, **k):
        return rebind_ok

    def admit_moa_turn(*a, **k):
        if admit_raises:
            raise RuntimeError("admit failed")
        return admit_ok

    def moa_override(p):
        return "meta"

    def t(key, **kw):
        return {
            "gateway.moa.managed_session": managed_text,
            "gateway.moa.prepare_failed": failed_text,
        }.get(key, key)

    stubmod("hermes_cli.moa_config", moa_usage=moa_usage)
    stubmod("gateway.session_moa_turn", default_preset=default_preset,
            admit_moa_turn=admit_moa_turn, moa_override=moa_override)
    stubmod("gateway.session_policy", policy_for_source=policy_for_source)
    stubmod("gateway.session_cli_turn", rebind_model_override=rebind_model_override)
    stubmod("gateway.session_command_sink", active_sink=active_sink)
    stubmod("gateway.session_managed_worker", policy_if_managed=policy_if_managed)
    live.t = t

    class Ev:
        def __init__(self):
            self.text = None
            self.message_id = "m1"

        def get_command_args(self):
            return args

    class Self:
        def _session_state(self, k):
            return types.SimpleNamespace(conversation=types.SimpleNamespace(model_override=None))

        def _claim_one_turn_restore(self, k):
            pass

        def _evict_cached_agent(self, k):
            pass

    ev = Ev()
    ret = asyncio.run(live.GatewayInboundMoaMixin._hm_cmd_moa(Self(), ev, object(), "qk"))
    return {"handled": ret[0], "reply": ret[1], "event_text": ev.text}


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except BaseException as ex:
        py = {"__raise__": type(ex).__name__, "message": str(ex)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_runinbound_bcb6b7: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
