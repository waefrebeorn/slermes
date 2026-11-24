#!/usr/bin/env python3
"""Oracle for gate w18_redactcred_108d32 (agent/redact_credential_stores.py)."""
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
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

live = _load("agent.redact_credential_stores", "agent/redact_credential_stores.py")

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
    text = r.get("text", "")
    if func == "aws_keys":
        py = [list(s) for s in live.aws_credential_process_keys(text)]
    elif func == "outside":
        spans = [tuple(x) for x in (r.get("spans") or [])]
        py = live.outside_spans(text, spans, lambda s: s.upper())
    elif func == "constants":
        py = {
            "_SECRET_CFG_NAMES": live._SECRET_CFG_NAMES,
            "_LINE_NUMBER_GUTTER": live._LINE_NUMBER_GUTTER,
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_redactcred_108d32: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
