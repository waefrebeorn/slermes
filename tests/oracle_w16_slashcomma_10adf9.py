#!/usr/bin/env python3
"""Differential oracle for w16_slashcomma_10adf9 (gateway/slash_commands_goals.py).

AST-extracts the LIVE _mgr_call and _quiet_bool into a registered module
namespace and drives them with scripted collaborators. loop_route mirrors the
exact route-building block of live _handle_loop_command (which cannot be
imported standalone: it needs the whole GatewayRunner).
"""
import __future__
import ast
import builtins
import json
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "gateway" / "slash_commands_goals.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "gateway" / "slash_commands_goals.py")
NAME = "w16_slashcomma_10adf9"

TREE = ast.parse(Path(SRC).read_text(encoding="utf-8"))
FLAGS = __future__.annotations.compiler_flag

NS = {"__builtins__": builtins}
for node in TREE.body:
    if isinstance(node, ast.FunctionDef) and node.name in {"_mgr_call", "_quiet_bool"}:
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=FLAGS, dont_inherit=True), NS)
MOD = types.ModuleType("wlive_slashgoals")
MOD.__dict__.update(NS)
sys.modules["wlive_slashgoals"] = MOD
NS = MOD.__dict__
if "_mgr_call" not in NS or "_quiet_bool" not in NS:
    print(f"ORACLE {NAME}: FATAL missing live defs", file=sys.stderr)
    sys.exit(1)

ROUTE_KEYS = ("chat_id", "chat_type", "thread_id", "user_id", "user_name", "profile")

def loop_route(src):
    platform = src.get("platform", "")
    if isinstance(platform, dict) and "value" in platform:
        pval = platform["value"]
    else:
        pval = str(platform or "")
    route = {"platform": pval}
    for key in ROUTE_KEYS:
        route[key] = str(src.get(key, "") or "")
    return {k: v for k, v in route.items() if v}

def drive(rec):
    func = rec["func"]
    if func == "mgr_call":
        ec, em = rec.get("exc_class"), rec.get("exc_msg", "")
        result = rec.get("result")
        def fn(*a):
            if ec:
                cls = getattr(builtins, ec, Exception)
                raise cls(em)
            return result
        return list(NS["_mgr_call"](rec.get("prefix", ""), fn))
    if func == "quiet_bool":
        value, raises = rec.get("value"), bool(rec.get("raises", False))
        def fn():
            if raises:
                raise RuntimeError("boom")
            return value
        return bool(NS["_quiet_bool"](fn))
    if func == "loop_route":
        return loop_route(rec.get("src") or {})
    raise KeyError(func)

def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        try:
            out = drive(rec)
        except BaseException as e:
            out = {"__raise__": type(e).__name__}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:600]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:600]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)

if __name__ == "__main__":
    main()
