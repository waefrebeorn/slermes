#!/usr/bin/env python3
"""Differential oracle for upstream Hermes pm/receipt.py (gate w5_receipt_6c967d).

The live module drives its real ContextVar state machine with a frozen clock
and a no-op receipt writer; each case runs in a fresh Context so state cannot
leak. The C port returns the equivalent receipt as pure JSON.
"""
import contextvars
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/pm/receipt.py"
spec = importlib.util.spec_from_file_location("w5_receipt_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_receipt_live"] = live
spec.loader.exec_module(live)

live._utc_now_iso = lambda: "T"
live._write_rotated = lambda data: "fake-path"

def apply_op(op):
    k = op.get("op")
    if k == "step":
        live.record_step(op.get("name", ""), op.get("ok", False), op.get("detail", ""))
    elif k == "rebuild":
        live.record_venv_rebuild(op.get("ok", False), op.get("reason", ""))
    elif k == "features":
        live.record_feature_list(op.get("extras"))
    elif k == "platform":
        live.record_platform(op.get("id", ""))
    elif k == "warning":
        live.record_warning(op.get("message", ""))
    elif k == "refusal":
        live.record_refusal(op.get("code", ""), op.get("detail", ""))

def run(kind, update_id, ops, outcome, exit_code, finalize):
    def body():
        live._worker_update.set((update_id,))
        token = live.begin(kind)
        for op in ops:
            apply_op(op)
        if finalize:
            live.finalize(outcome, exit_code)
            return live.last_completed()
        py = live.snapshot()
        live._current.reset(token)
        return py
    return contextvars.Context().run(body)

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
    args = {k: v for k, v in r.items() if k not in ("func", "out")}
    try:
        if func == "receipt":
            py = run(args.get("kind", ""), args.get("update_id"),
                     args.get("ops") or [], "", 0, False)
        elif func == "finalized":
            py = run(args.get("kind", ""), args.get("update_id"),
                     args.get("ops") or [], args.get("outcome", ""),
                     args.get("exit_code", 0), True)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR func={func} args={args}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH func={func} args={args}\n  py={py!r}\n   c={got!r}", file=sys.stderr)

print(f"ORACLE w5_receipt_6c967d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
