#!/usr/bin/env python3
"""Oracle for upstream Hermes persistence/workers/receipts.py (WorkerReceiptReads Protocol)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

ROOT = "C:/Users/eman5/hermes-agent"
SRC = ROOT + "/persistence/workers/receipts.py"
DOTTED = "persistence.workers.receipts"
sys.path.insert(0, ROOT)
spec = importlib.util.spec_from_file_location(DOTTED, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[DOTTED] = live
spec.loader.exec_module(live)

P = live.WorkerReceiptReads
METHODS = sorted(n for n, v in vars(P).items()
                 if not n.startswith("_") and callable(v))

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
    if func == "protocol_name":
        py = P.__name__
    elif func == "is_protocol":
        py = bool(getattr(P, "_is_protocol", False))
    elif func == "method_count":
        py = len(METHODS)
    elif func == "method_at":
        i = int(r.get("i", 0))
        py = METHODS[i] if 0 <= i < len(METHODS) else None
    elif func == "has_method":
        py = bool(hasattr(P, r.get("name", "")))
    elif func == "stub":
        # Protocol bodies are literally `...`; calling them yields None regardless
        # of the dummy keyword arguments the signatures demand.
        name = r.get("name", "")
        if name == "claim_origin":
            py = P.claim_origin(None, execution_id="e", session_id="s")
        elif name == "publication_target":
            py = P.publication_target(None, execution_id="e", session_id="s",
                                      generation=None, sequence=0,
                                      operation="", payload={})
        else:
            py = None
    else:
        continue

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE receipts: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
