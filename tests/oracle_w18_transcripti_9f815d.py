#!/usr/bin/env python3
"""Oracle for gate w18_transcripti_9f815d (tools/transcription_streaming.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    # module-level imports: agent.memory_provider.spawn_context_thread, utils.is_truthy_value
    agent_mod = sys.modules.get("agent") or types.ModuleType("agent")
    agent_mod.__path__ = [QUARRY + "/agent"]
    sys.modules["agent"] = agent_mod
    mp = types.ModuleType("agent.memory_provider")
    mp.spawn_context_thread = lambda *a, **k: types.SimpleNamespace(start=lambda: None)
    sys.modules["agent.memory_provider"] = mp
    utils = types.ModuleType("utils")
    utils.is_truthy_value = lambda v, default=False: bool(v) if v is not None else default
    sys.modules["utils"] = utils
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m


live = _load("tools.transcription_streaming", "tools/transcription_streaming.py")

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
        if func == "resample":
            sr = int(r.get("src_rate", 16000))
            dst = int(r.get("dst_rate", 16000))
            rs = live.Resampler(sr, dst)
            acc = b""
            for c in (r.get("chunks") or []):
                acc += rs(bytes.fromhex(c))
            py = acc.hex()
        elif func == "error":
            s = live.StreamingSession()
            s.provider = r.get("provider", "unknown")
            py = s._error(r.get("message", ""))
        else:
            continue
    except Exception as exc:
        print(f"SKIP {func}: {exc}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_transcripti_9f815d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
