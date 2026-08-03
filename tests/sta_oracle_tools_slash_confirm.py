#!/usr/bin/env python3
"""Oracle: tools/slash_confirm.py vs LIVE Python (newline-delimited JSON)."""
import json, sys, os, types, asyncio
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("sc_mod", "/home/wubu/hermes-agent-dev/tools/slash_confirm.py")
mod = importlib.util.module_from_spec(spec)
# stub heavy deps (threading/logging fine; asyncio fine)
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

async def fake_handler(choice):
    return f"handled:{choice}"

mm = 0
# reset module state
for k in list(mod._pending.keys()):
    del mod._pending[k]

for line in sys.stdin:
    line = line.strip()
    if not line: continue
    b = json.loads(line)
    def norm_exp(exp):
        """Compare only the serializable contract (confirm_id + command) —
        handler is a live function and created_at is a wall-clock float, so
        neither is part of the C port's JSON surface."""
        if exp is None:
            return None
        return {"confirm_id": exp.get("confirm_id"), "command": exp.get("command")}
    def norm_got(got):
        if got is None:
            return None
        o = json.loads(got)
        return {"confirm_id": o.get("confirm_id"), "command": o.get("command")}
    if b["t"] == "get_none":
        exp = norm_exp(mod.get_pending("sess1"))
        got = norm_got(b["out"])
        if exp != got: mm += 1; print(f"MISMATCH get_none: exp {exp!r} got {got!r}")
    elif b["t"] == "get":
        mod.register("sess1", "cid-1", "reload-mcp", fake_handler)
        exp = norm_exp(mod.get_pending("sess1"))
        got = norm_got(b["out"])
        if exp != got: mm += 1; print(f"MISMATCH get: exp {exp!r} got {got!r}")
    elif b["t"] == "resolve":
        exp = asyncio.get_event_loop().run_until_complete(mod.resolve("sess1", "cid-1", "once"))
        got = b["out"]
        if exp != got: mm += 1; print(f"MISMATCH resolve: exp {exp!r} got {got!r}")
    elif b["t"] == "get_after_resolve":
        exp = norm_exp(mod.get_pending("sess1"))
        got = norm_got(b["out"])
        if exp != got: mm += 1; print(f"MISMATCH get_after_resolve: exp {exp!r} got {got!r}")
    elif b["t"] == "resolve_wrong":
        mod.register("sess2", "cid-A", "reload-mcp", fake_handler)
        exp = asyncio.get_event_loop().run_until_complete(mod.resolve("sess2", "cid-WRONG", "once"))
        got = b["out"]
        if exp != got: mm += 1; print(f"MISMATCH resolve_wrong: exp {exp!r} got {got!r}")
    elif b["t"] == "get_after_clear":
        mod.clear("sess2")
        exp = norm_exp(mod.get_pending("sess2"))
        got = norm_got(b["out"])
        if exp != got: mm += 1; print(f"MISMATCH get_after_clear: exp {exp!r} got {got!r}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")
